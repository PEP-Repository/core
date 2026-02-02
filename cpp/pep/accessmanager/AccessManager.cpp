#include <pep/accessmanager/AccessManager.hpp>
#include <pep/accessmanager/Backend.hpp>

#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/accessmanager/AmaSerializers.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/auth/EnrolledParty.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/elgamal/CurvePoint.PropertySerializer.hpp>
#include <pep/morphing/RepoKeys.hpp>
#include <pep/morphing/RepoRecipient.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/rsk/RskSerializers.hpp>
#include <pep/structure/ColumnNameSerializers.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Filesystem.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/accessmanager/UserSerializers.hpp>

#include <numeric>
#include <sstream>
#include <chrono>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-concat.hpp>

#include <prometheus/gauge.h>
#include <prometheus/summary.h>

namespace pep {

namespace {

void FillTranscryptorRequestEntry(
    TranscryptorRequestEntry& entry,
    const PseudonymTranslator& pseudonymTranslator
) {
  std::tie(entry.mAccessManager, entry.mAccessManagerProof) =
      pseudonymTranslator.certifiedTranslateStep(
          entry.mPolymorphic,
          RecipientForServer(EnrolledParty::AccessManager));
  std::tie(entry.mStorageFacility, entry.mStorageFacilityProof) =
      pseudonymTranslator.certifiedTranslateStep(
          entry.mPolymorphic,
          RecipientForServer(EnrolledParty::StorageFacility));
  std::tie(entry.mTranscryptor, entry.mTranscryptorProof) =
      pseudonymTranslator.certifiedTranslateStep(
          entry.mPolymorphic,
          RecipientForServer(EnrolledParty::Transcryptor));
  entry.ensurePacked();
}

const std::string LOG_TAG ("AccessManager");
const severity_level TICKET_REQUEST_LOGGING_SEVERITY = debug;

constexpr size_t TS_REQUEST_BATCH_SIZE = 400U;

const size_t MAX_AMA_QUERY_RESPONSE_STRINGS = 25000; // See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2089#note_25719
const std::size_t AMA_QUERY_RESPONSE_STRINGS_WARNING_THRESHOLD = static_cast<std::size_t>(0.8 * MAX_AMA_QUERY_RESPONSE_STRINGS);

void FillTranscryptorRequestEntry(
    TranscryptorRequestEntry& entry,
    const PseudonymTranslator& pseudonymTranslator,
    bool includeUserGroupPseudonyms,
    const Signature& signature
) {
  if (includeUserGroupPseudonyms) {
    std::tie(entry.mUserGroup.emplace(), entry.mUserGroupProof.emplace()) =
        pseudonymTranslator.certifiedTranslateStep(
            entry.mPolymorphic,
            RecipientForCertificate(signature.getLeafCertificate()));
  }
  FillTranscryptorRequestEntry(entry, pseudonymTranslator);
}

constexpr size_t CountAmaQueryResponseEntryStrings(const AmaQRColumn&) {
  return 1U;
}
constexpr size_t CountAmaQueryResponseEntryStrings(const AmaQRColumnGroupAccessRule&) {
  return 3U;
}
constexpr size_t CountAmaQueryResponseEntryStrings(const AmaQRParticipantGroup&) {
  return 1U;
}
constexpr size_t CountAmaQueryResponseEntryStrings(const AmaQRParticipantGroupAccessRule&) {
  return 3U;
}

template <typename T>
std::vector<AmaQueryResponse> ExtractPartialQueryResponse(const AmaQueryResponse& source, std::vector<T> AmaQueryResponse::* member) {
  const auto& sourceEntries = source.*member;

  std::vector<AmaQueryResponse> responses; // The (partial) AmaQueryResponse items that we'll send out
  std::vector<T>* responseEntries = nullptr; // The AmaQRxyz entries within the AmaQueryResponse that's being filled
  size_t responseStrings = MAX_AMA_QUERY_RESPONSE_STRINGS + 1U; // Mark "previous response full" to have first AmaQueryResponse created

  // TODO: use more efficient chunking for T with fixed number of strings
  for (size_t i = 0U; i < sourceEntries.size(); ++i) {
    // Get source AmaQRxyz entry and check whether it'll fit in a message at all
    const T& entry = sourceEntries[i];
    auto entryStrings = CountAmaQueryResponseEntryStrings(entry);
    if (entryStrings > AMA_QUERY_RESPONSE_STRINGS_WARNING_THRESHOLD) {
      LOG(LOG_TAG, warning) << "(Excessively) large AMA query response entry: " << NormalizedTypeNamer<T>::GetTypeName() << " contains " + std::to_string(entryStrings) + " strings";
    }

    // Create a new  if the entry can't be added to the one that's being filled
    if (responseStrings + entryStrings > MAX_AMA_QUERY_RESPONSE_STRINGS) { // This entry can't be added to the AmaQueryResponse that we're currently filling
      // Create a new AmaQueryResponse and initialize stuff for it to be filled
      auto& response = responses.emplace_back();
      responseEntries = &(response.*member);
      responseEntries->reserve(MAX_AMA_QUERY_RESPONSE_STRINGS);
      responseStrings = 0U;
    }

    // Add the (source) AmaQRxyz entry to the (partial) AmaQueryResponse that we're currently filling
    assert(responseEntries != nullptr);
    responseEntries->push_back(entry);
    responseStrings += entryStrings;
  }

  // Reclaim reserved-but-unused space
  std::for_each(responses.begin(), responses.end(), [member](AmaQueryResponse& response) {(response.*member).shrink_to_fit(); });

  return responses;
}

auto MakeStreamWithDeferredCleanup(const std::filesystem::path& path) {
  auto stream = std::make_shared<std::ofstream>(path, std::ios::binary);
  auto deferredCleanup = defer_shared([stream, path]() {
    stream->close();
    std::filesystem::remove(path);
  });
  return std::make_pair(stream, deferredCleanup);
}

} // namespace

AccessManager::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry) :
  RegisteredMetrics(registry),
  enckey_request_duration(prometheus::BuildSummary()
    .Name("pep_accessmanager_enckey_request_duration_seconds")
    .Help("Duration of a successful encryptionkey request")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}, std::chrono::minutes{5})),
      ticket_request2_duration(prometheus::BuildSummary()
      .Name("pep_accessmanager_ticket_request2_duration_seconds")
      .Help("Duration of a successful ticket2 request")
      .Register(*registry)
      .Add({}, prometheus::Summary::Quantiles{
        {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}, std::chrono::minutes{5})),
        keyComponent_request_duration(prometheus::BuildSummary()
        .Name("pep_accessmanager_keyComponent_request_duration_seconds")
        .Help("Duration of a successful keyComponent request")
        .Register(*registry)
        .Add({}, prometheus::Summary::Quantiles{
          {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}, std::chrono::minutes{5})),
          ticket_request_duration(prometheus::BuildSummary()
          .Name("pep_accessmanager_ticket_request_duration_seconds")
            .Help("Duration of a successful ticket request")
            .Register(*registry)
            .Add({}, prometheus::Summary::Quantiles{
              {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}, std::chrono::minutes{5})) {}


AccessManager::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : SigningServer::Parameters(io_context, config) {
  std::filesystem::path keysFile;
  std::filesystem::path globalConfFile;
  ElgamalPublicKey publicKeyPseudonyms;

  std::filesystem::path systemKeysFile;

  std::string strPseudonymKey;

  std::filesystem::path storageFile;

  try {
    keysFile = config.get<std::filesystem::path>("KeysFile");
    globalConfFile = config.get<std::filesystem::path>("GlobalConfigurationFile");

    publicKeyPseudonyms = config.get<ElgamalPublicKey>("PublicKeyPseudonyms");
    transcryptorEndPoint = config.get<EndPoint>(ServerTraits::Transcryptor().configNode());
    keyServerEndPoint = config.get<EndPoint>(ServerTraits::KeyServer().configNode());

    if (auto optionalSystemKeysFile = config.get<std::optional<std::filesystem::path>>("SystemKeysFile")) {
      systemKeysFile = optionalSystemKeysFile.value();
    }
    else {
      //Legacy version, from when we still had a (Soft)HSM. TODO: use new version in configuration for all environments, and remove legacy version.
      systemKeysFile = config.get<std::filesystem::path>("HSM.ConfigFile");
    }

    storageFile = config.get<std::filesystem::path>("StorageFile");
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with configuration file: " << e.what();
    throw;
  }

  try {
    Configuration keysConfig = Configuration::FromFile(keysFile);
    strPseudonymKey = boost::algorithm::unhex(keysConfig.get<std::string>("PseudonymKey"));
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with keys file: " << keysFile << " : " << e.what();
    throw;
  }

  boost::property_tree::ptree systemKeys;
  boost::property_tree::read_json(std::filesystem::canonical(systemKeysFile).string(), systemKeys);
  systemKeys = systemKeys.get_child_optional("Keys") //Old HSMKeys.json files have the keys in a Keys-object
      .get_value_or(systemKeys); //we now also allow them to be directly in the root, resulting in cleaner SystemKeys-files
  setPseudonymTranslator(std::make_shared<PseudonymTranslator>(ParsePseudonymTranslationKeys(systemKeys)));
  setDataTranslator(std::make_shared<DataTranslator>(ParseDataTranslationKeys(systemKeys)));

  setPseudonymKey(ElgamalPrivateKey(strPseudonymKey));
  setPublicKeyPseudonyms(publicKeyPseudonyms);

  auto globalConf = std::make_shared<GlobalConfiguration>(
    Serialization::FromJsonString<GlobalConfiguration>(
      ReadFile(globalConfFile)));
  setGlobalConfiguration(globalConf);
  setBackend(std::make_shared<AccessManager::Backend>(storageFile, globalConf));
}

void AccessManager::Parameters::setGlobalConfiguration(std::shared_ptr<GlobalConfiguration> gc) {
  const auto& contexts = gc->getStudyContexts().getItems();
  auto contexts_end = contexts.cend();
  auto end = gc->getShortPseudonyms().cend();
  for (auto i = gc->getShortPseudonyms().cbegin(); i != end; ++i) {
    if (!contexts.empty()) {
      if (contexts_end == std::find_if(contexts.cbegin(), contexts_end, [&i](const StudyContext& candidate) {return candidate.matchesShortPseudonym(*i); })) {
        throw std::runtime_error("Short pseudonym " + i->getColumn().getFullName() + " defined for unknown study context " + i->getStudyContext());
      }
    }

    if (i->getCastor()) {
      const auto& storage = i->getCastor()->getStorageDefinitions();
      auto end = storage.cend();
      for (auto s = storage.cbegin(); s != end; ++s) {
        for (auto s2 = s + 1; s2 != end; ++s2) {
          if ((*s)->getDataColumn() == (*s2)->getDataColumn()) {
            throw std::runtime_error("Short pseudonym definitions contain duplicate Castor storage data columns: " + (*s)->getDataColumn());
          }
        }
      }
    }

    for (auto j = i + 1; j != end; ++j) {
      if (i->getColumn().getFullName() == j->getColumn().getFullName()) {
        throw std::runtime_error("Short pseudonym definitions contain duplicate column names: " + i->getColumn().getFullName());
      }
      if (i->getPrefix() == j->getPrefix()) {
        throw std::runtime_error("Short pseudonym definitions contain duplicate prefixes: " + i->getPrefix());
      }
      if (i->getCastor() && j->getCastor()) {
        if (i->getCastor()->getStudySlug() == j->getCastor()->getStudySlug()
          && i->getCastor()->getSiteAbbreviation() != j->getCastor()->getSiteAbbreviation()) {
          throw std::runtime_error("Castor study slug " + i->getCastor()->getStudySlug() + " is configured with multiple site abbreviations: "
            + i->getCastor()->getSiteAbbreviation() + " and " + j->getCastor()->getSiteAbbreviation());
        }
        for (auto& i_storage : i->getCastor()->getStorageDefinitions()) {
          for (auto& j_storage : j->getCastor()->getStorageDefinitions()) {
            if (i_storage->getDataColumn() == j_storage->getDataColumn()) {
              throw std::runtime_error("Short pseudonym definitions contain duplicate Castor storage data columns: " + i_storage->getDataColumn());
            }
          }
        }
      }
    }
  }
  this->globalConf = gc;
}

std::shared_ptr<GlobalConfiguration> AccessManager::Parameters::getGlobalConfiguration() const {
  return this->globalConf;
}

/*!
  * \return The pseudonym key
  */
const ElgamalPrivateKey& AccessManager::Parameters::getPseudonymKey() const {
  return pseudonymKey.value();
}

const ElgamalPublicKey& AccessManager::Parameters::getPublicKeyPseudonyms() const {
  return publicKeyPseudonyms.value();
}
/*!
  * \param pseudonymKey The pseudonym key
  */
void AccessManager::Parameters::setPseudonymKey(const ElgamalPrivateKey& pseudonymKey) {
  Parameters::pseudonymKey = pseudonymKey;
}

void AccessManager::Parameters::setPublicKeyPseudonyms(
  const ElgamalPublicKey& pk) {
  Parameters::publicKeyPseudonyms = pk;
}

const EndPoint& AccessManager::Parameters::getTranscryptorEndPoint() const {
  return transcryptorEndPoint;
}

const EndPoint& AccessManager::Parameters::getKeyServerEndPoint() const {
  return keyServerEndPoint;
}

std::shared_ptr<PseudonymTranslator> AccessManager::Parameters::getPseudonymTranslator() const {
  return pseudonymTranslator;
}

std::shared_ptr<DataTranslator> AccessManager::Parameters::getDataTranslator() const {
  return dataTranslator;
}

void AccessManager::Parameters::setPseudonymTranslator(std::shared_ptr<PseudonymTranslator> pseudonymTranslator) {
  Parameters::pseudonymTranslator = pseudonymTranslator;
}

void AccessManager::Parameters::setDataTranslator(std::shared_ptr<DataTranslator> dataTranslator) {
  Parameters::dataTranslator = dataTranslator;
}

std::shared_ptr<AccessManager::Backend> AccessManager::Parameters::getBackend() const {
  return backend;
}
void AccessManager::Parameters::setBackend(std::shared_ptr<AccessManager::Backend> backend) {
  Parameters::backend = backend;
}

void AccessManager::Parameters::check() const {
  if (!pseudonymKey)
    throw std::runtime_error("pseudonymKey must be set");
  if (!publicKeyPseudonyms)
    throw std::runtime_error("publicKeyPseudonyms must be set");
  if(!pseudonymTranslator)
    throw std::runtime_error("pseudonymTranslator must be set");
  if(!dataTranslator)
    throw std::runtime_error("dataTranslator must be set");
  if (!backend)
    throw std::runtime_error("backend must be set");

  SigningServer::Parameters::check();
}

messaging::MessageBatches AccessManager::handleKeyComponentRequest(std::shared_ptr<SignedKeyComponentRequest> pRequest) {
  // Generate response
  auto start_time = std::chrono::steady_clock::now();
  auto response = KeyComponentResponse::HandleRequest(
    *pRequest,
    *mPseudonymTranslator,
    *mDataTranslator,
    *this->getRootCAs());
  lpMetrics->keyComponent_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count());

  //Return result
  return messaging::BatchSingleMessage(response);
}

AccessManager::AccessManager(std::shared_ptr<AccessManager::Parameters> parameters)
  : SigningServer(parameters),
  mPseudonymKey(parameters->getPseudonymKey()),
  mPublicKeyPseudonyms(parameters->getPublicKeyPseudonyms()),
  mTranscryptorProxy(messaging::ServerConnection::Create(this->getIoContext(), parameters->getTranscryptorEndPoint(), parameters->getRootCACertificatesFilePath()), *this, parameters->getTranscryptorEndPoint().expectedCommonName, getRootCAs()),
  mKeyServerProxy(messaging::ServerConnection::Create(this->getIoContext(), parameters->getKeyServerEndPoint(), parameters->getRootCACertificatesFilePath()), *this),
  mPseudonymTranslator(parameters->getPseudonymTranslator()),
  mDataTranslator(parameters->getDataTranslator()),
  backend(parameters->getBackend()),
  globalConf(parameters->getGlobalConfiguration()),
  lpMetrics(std::make_shared<AccessManager::Metrics>(mRegistry)),
  mWorkerPool(WorkerPool::getShared()) {
  backend->setAccessManager(this);
  RegisterRequestHandlers(*this,
                          &AccessManager::handleKeyComponentRequest,
                          &AccessManager::handleTicketRequest2,
                          &AccessManager::handleEncryptionKeyRequest,
                          &AccessManager::handleGlobalConfigurationRequest,
                          &AccessManager::handleAmaMutationRequest,
                          &AccessManager::handleAmaQuery,
                          &AccessManager::handleUserQuery,
                          &AccessManager::handleUserMutationRequest,
                          &AccessManager::handleVerifiersRequest,
                          &AccessManager::handleColumnAccessRequest,
                          &AccessManager::handleParticipantGroupAccessRequest,
                          &AccessManager::handleColumnNameMappingRequest,
                          &AccessManager::handleFindUserRequest,
                          &AccessManager::handleMigrateUserDbToAccessManagerRequest,
                          &AccessManager::handleStructureMetadataRequest,
                          &AccessManager::handleSetStructureMetadataRequest);
}

std::optional<std::filesystem::path> AccessManager::getStoragePath() {
  return EnsureDirectoryPath(backend->getStoragePath().parent_path());
}

std::unordered_set<std::string> AccessManager::getAllowedChecksumChainRequesters() {
  auto result = Server::getAllowedChecksumChainRequesters();
  for (auto& authserver : UserGroup::Authserver) result.insert(authserver);
  return result;
}

messaging::MessageBatches AccessManager::handleGlobalConfigurationRequest(std::shared_ptr<GlobalConfigurationRequest> request) {
  return messaging::BatchSingleMessage(*globalConf);
}

messaging::MessageBatches
AccessManager::handleEncryptionKeyRequest(std::shared_ptr<SignedEncryptionKeyRequest> signedRequest) {
  auto start_time = std::chrono::steady_clock::now();
  auto userGroup = signedRequest->getLeafCertificateOrganizationalUnit();
  auto request = std::make_shared<EncryptionKeyRequest>(
    signedRequest->open(*this->getRootCAs()));
  const auto party = GetEnrolledParty(signedRequest->mSignature.mCertificateChain);
  if (!party.has_value()) {
    throw std::runtime_error("Cannot produce encryption key for this requestor");
  }
  if (!HasDataAccess(*party)) {
    throw std::runtime_error("Unsupported enrolled party " + std::to_string(static_cast<unsigned>(*party)));
  }

  const auto recipient = std::make_shared<RekeyRecipient>(
    RekeyRecipientForCertificate(signedRequest->getLeafCertificate()));

  if (request->mTicket2 == nullptr)
    throw Error("Invalid signature or missing ticket");

  auto ticket = request->mTicket2->open(*this->getRootCAs(), userGroup);

  backend->checkTicketForEncryptionKeyRequest(request, ticket);

  // Note that it is clear that the client has access to the given
  // participants as their polymorphic pseudonyms are taken from the
  // list in the signed ticket.

  auto lpResponse = std::make_shared<EncryptionKeyResponse>();
  lpResponse->mKeys.resize(request->mEntries.size());


  size_t dwNumUnblind = 0;
  for (const auto& entry : request->mEntries)
    if (entry.mKeyBlindMode == KeyBlindMode::BLIND_MODE_UNBLIND)
      dwNumUnblind++;

  // Decrypt local pseudonyms
  auto server = SharedFrom(*this);
  auto localPseudonyms = std::make_shared<std::vector<LocalPseudonym>>();
  return server->mWorkerPool->batched_map<8>(ticket.mPseudonyms,
        observe_on_asio(*server->getIoContext()),
        [server, localPseudonyms](LocalPseudonyms elp) -> LocalPseudonym {
          return elp.mAccessManager.decrypt(server->mPseudonymKey);
        })
      .flat_map([start_time, server, dwNumUnblind, lpResponse, request, signedRequest, recipient, localPseudonyms
        ](std::vector<LocalPseudonym> localPseudonymsOnStack) {
          *localPseudonyms = std::move(localPseudonymsOnStack);
          return server->mWorkerPool->batched_map<8>(request->mEntries,
                observe_on_asio(*server->getIoContext()),
                [server, localPseudonyms](KeyRequestEntry entry) {
                  EncryptedKey key;

                  if (entry.mKeyBlindMode == KeyBlindMode::BLIND_MODE_BLIND) {
                    LocalPseudonym mLocalPseudonym;
                    try {
                      mLocalPseudonym = localPseudonyms->at(entry.mPseudonymIndex);
                    } catch (const std::out_of_range&) {
                      LOG(LOG_TAG, critical) << "Out of bounds read on local pseudonyms vector during key blinding";
                      throw;
                    }
                    auto blindingAD = entry.mMetadata.computeKeyBlindingAdditionalData(mLocalPseudonym);
                    key = server->mDataTranslator->blind(
                      entry.mPolymorphEncryptionKey,
                      blindingAD.content,
                      blindingAD.invertComponent
                    );
                    key.ensurePacked();
                  } else if (entry.mKeyBlindMode == KeyBlindMode::BLIND_MODE_UNBLIND) {
                    // do nothing --- we need the transcryptor to help out
                  } else {
                    std::ostringstream msg;
                    msg << "Received unknown blinding mode: " << entry.mKeyBlindMode;
                    throw Error(msg.str());
                  }
                  return key;
                })
              .flat_map([start_time, server, dwNumUnblind, lpResponse, request, signedRequest, recipient, localPseudonyms
                ](std::vector<EncryptedKey> keys) -> messaging::MessageBatches {
                  lpResponse->mKeys = std::move(keys);

                  if (dwNumUnblind == 0) {
                    server->lpMetrics->enckey_request_duration.Observe(
                      std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count());
                    return messaging::BatchSingleMessage(*lpResponse);
                  }

                  /* if we find at least one unblind entry in the request
                    * we can't deal with this ourselves, we need the transcryptor for this
                    */
                  LOG(LOG_TAG, debug) << "Rekey request has a BLIND_MODE_UNBLIND entry -> forwarding to transcryptor";
                  RekeyRequest rkReq{
                    .mKeys{},
                    .mClientCertificateChain = signedRequest->mSignature.mCertificateChain,
                  };
                  rkReq.mKeys.reserve(dwNumUnblind);

                  // Index of the entry into Rekey{Request,Response}.
                  auto rkIndices = std::make_shared<std::vector<uint32_t>>(request->mEntries.size());

                  for (size_t i = 0; i < request->mEntries.size(); i++) {
                    auto& entry = request->mEntries[i];
                    if (entry.mKeyBlindMode != KeyBlindMode::BLIND_MODE_UNBLIND)
                      continue;
                    try {
                      rkIndices->at(i) = static_cast<uint32_t>(rkReq.mKeys.size());
                    } catch (const std::out_of_range&) {
                      LOG(LOG_TAG, critical) << "Out of bounds read on rekey indices vector during key unblinding";
                      throw;
                    }
                    rkReq.mKeys.push_back(entry.mPolymorphEncryptionKey);
                  }

                  return server->mTranscryptorProxy.requestRekey(std::move(rkReq)).flat_map(
                    [server, rkIndices, start_time, request, signedRequest, recipient, lpResponse, localPseudonyms
                    ](RekeyResponse transRespOnStack) {

                      auto transResp = std::make_shared<RekeyResponse>(std::move(transRespOnStack));

                      // mWorkerPool->batched_map() does not tell us which index we're handling,
                      // so we let it process indices to work around this.  If we need this
                      // more often, it's better to change batched_map()
                      std::vector<size_t> is(request->mEntries.size());
                      std::iota(is.begin(), is.end(), 0);
                      return server->mWorkerPool->batched_map<8>(is,
                            observe_on_asio(*server->getIoContext()),
                            [server, request, lpResponse, signedRequest, transResp, rkIndices, localPseudonyms, recipient
                            ](size_t i) {
                              auto& entry = request->mEntries[i];
                              auto& key = lpResponse->mKeys[i];

                              if (entry.mKeyBlindMode != KeyBlindMode::BLIND_MODE_UNBLIND)
                                return i; // we have to return something

                              //TODO: check access once access is based on local pseudonyms
                              LocalPseudonym lp;
                              try {
                                lp = localPseudonyms->at(entry.mPseudonymIndex);
                              } catch (const std::out_of_range&) {
                                LOG(LOG_TAG, critical) << "Out of bounds read on local pseudonyms vector during key unblinding";
                                throw;
                              }
                              auto blindingAD = entry.mMetadata.computeKeyBlindingAdditionalData(lp);

                              EncryptedKey encryptedKey;
                              try {
                                encryptedKey = transResp->mKeys.at((*rkIndices)[i]);
                              } catch (const std::out_of_range&) {
                                LOG(LOG_TAG, critical) << "Out of bounds read on keys vector during unblinding-and-rekeying";
                                throw;
                              }

                              key = server->mDataTranslator->unblindAndTranslate(
                                encryptedKey,
                                blindingAD.content,
                                blindingAD.invertComponent,
                                *recipient);
                              key.ensurePacked();
                              return i; // we have to return something
                            })
                          .map([server, lpResponse, start_time](std::vector<size_t>) {
                            server->lpMetrics->enckey_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count());
                            return rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(*lpResponse))).as_dynamic();
                          });
                    });
                });
        });
}

std::vector<std::string> AccessManager::getChecksumChainNames() const {
  return backend->getChecksumChainNames();
}

void AccessManager::computeChecksumChainChecksum(
  const std::string& chain,
  std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
  uint64_t& checkpoint) {
  backend->computeChecksum(chain, maxCheckpoint, checksum, checkpoint);
}

messaging::MessageBatches
AccessManager::handleTicketRequest2(std::shared_ptr<SignedTicketRequest2> signedRequest) {
  auto time = std::chrono::steady_clock::now();
  auto requestNumber = mNextTicketRequestNumber++;

  LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << requestNumber << " received";

  // openAsAccessManager checks that mSignature and mLogSignature are set,
  // are valid and match.
  auto request = signedRequest->openAsAccessManager(*this->getRootCAs());
  auto userGroup = signedRequest->mSignature->getLeafCertificateOrganizationalUnit();

  backend->checkTicketRequest(request);

  auto timestamp = TimeNow();
  std::vector<std::string> modes{"access"};
  backend->checkParticipantGroupAccess(request.mParticipantGroups, userGroup, modes, timestamp);

  std::vector<AccessManager::Backend::pp_t> prePPs;
  prePPs.reserve(request.mPolymorphicPseudonyms.size());
  std::transform(request.mPolymorphicPseudonyms.cbegin(), request.mPolymorphicPseudonyms.cend(), std::back_inserter(prePPs), [](const PolymorphicPseudonym& pp) {return AccessManager::Backend::pp_t(pp, true); });

  std::unordered_map<std::string, IndexList> participantGroupMap;
  backend->fillParticipantGroupMap(request.mParticipantGroups, prePPs, participantGroupMap);

  // Prepare ticket

  Ticket2 ticket;
  ticket.mTimestamp = TimeNow();
  ticket.mModes = request.mModes;
  ticket.mColumns = request.mColumns;
  ticket.mUserGroup = userGroup;

  // Check columns and column groups
  std::unordered_map<std::string, IndexList> columnGroupMap;

  backend->unfoldColumnGroupsAndAssertAccess(userGroup, request.mColumnGroups, request.mModes, timestamp,
                                             ticket.mColumns,         // columns (in & out)
                                             columnGroupMap);         // (out)

  // Because of all the asynchronous IO, we move all state into this context
  // struct, so that we don't have to put everything into shared_ptrs
  struct Context {
    std::shared_ptr<AccessManager> server;
    uintmax_t requestNumber;
    TicketRequest2 request;
    Ticket2 ticket;
    SignedTicket2 signedTicket{};
    std::vector<AccessManager::Backend::pp_t> pps;
    decltype(time) start_time;
    std::unordered_map<std::string, IndexList> columnGroupMap;
    std::unordered_map<std::string, IndexList> participantGroupMap;
    std::vector<std::string> participantModes;
    TranscryptorRequest tsReq{};
    TranscryptorRequestEntries tsReqEntries{};
    Signature signature; // signature (for the AM) on the TicketRequest
  };

  auto ctx = MakeSharedCopy(Context{
    .server = SharedFrom(*this),
    .requestNumber = requestNumber,
    .request = std::move(request),
    .ticket = std::move(ticket),
    .pps = std::move(prePPs),
    .start_time = time,
    .columnGroupMap = std::move(columnGroupMap),
    .participantGroupMap = std::move(participantGroupMap),
    .participantModes = std::move(modes),
    .signature = std::move(*signedRequest->mSignature),
    });

  // Remove the main client signature to prevent reuse of
  // the SignedTicketRequest2.
  signedRequest->mSignature.reset();

  // Prepare transcryptor request
  ctx->tsReq.mRequest = std::move(*signedRequest);
  ctx->tsReqEntries.mEntries.resize(ctx->pps.size());

  LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << requestNumber << " constructing observable";

  // mWorkerPool->batched_map() does not tell us which index we're handling,
  // so we let it process indices to work around this.  If we need this
  // more often, it's better to change batched_map()
  std::vector<size_t> indexes(ctx->pps.size());
  std::iota(indexes.begin(), indexes.end(), 0);
  messaging::MessageBatches result =
    ctx->server->mWorkerPool->batched_map<8>(std::move(indexes),
        observe_on_asio(*ctx->server->getIoContext()),
      [ctx](size_t i) {
    AccessManager::Backend::pp_t& pp = ctx->pps[i];
    TranscryptorRequestEntry& entry = ctx->tsReqEntries.mEntries[i];

    // Rerandomize old PPs (ie. from the database)
    if (pp.isClientProvided)
      entry.mPolymorphic = pp.pp;
    else
      entry.mPolymorphic = pp.pp.rerandomize();

    FillTranscryptorRequestEntry(
        entry,
        *ctx->server->mPseudonymTranslator,
        ctx->request.mIncludeUserGroupPseudonyms,
        ctx->signature);
    return i;
  }).flat_map([ctx](std::vector<size_t> is) {
    // Send request to transcrypor
    auto tail = CreateObservable<messaging::TailSegment<TranscryptorRequestEntries>>([ctx](rxcpp::subscriber<messaging::TailSegment<TranscryptorRequestEntries>> subscriber) {
      size_t ibatch = 0U;
      for (size_t i = 0; i < ctx->tsReqEntries.mEntries.size(); i += TS_REQUEST_BATCH_SIZE) {
        ++ibatch;
        auto count = std::min(TS_REQUEST_BATCH_SIZE, ctx->tsReqEntries.mEntries.size() - i);
        using index = decltype(ctx->tsReqEntries.mEntries.cbegin())::difference_type;
        index first = static_cast<index>(i), end = static_cast<index>(i + count);
        TranscryptorRequestEntries batch;
        batch.mEntries.reserve(count);
        std::copy(ctx->tsReqEntries.mEntries.cbegin() + first, ctx->tsReqEntries.mEntries.cbegin() + end, std::back_inserter(batch.mEntries));
        LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " sending transcryptor request entry batch " << ibatch << " containing entries " << first << " through " << end;
        subscriber.on_next(messaging::MakeTailSegment(batch));
      }
      subscriber.on_completed();
      LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " sent " << ctx->tsReqEntries.mEntries.size() << " transcryptor request entries in " << ibatch << " batch(es)";
      });

    LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " sending transcryptor request";
    return ctx->server->mTranscryptorProxy.requestTranscryption(ctx->tsReq, tail);
  }).flat_map([ctx](TranscryptorResponse resp) {
    LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " received transcryptor response";
    // Now we have local pseudonyms for the orignal PPs.
    if (resp.mEntries.size() != ctx->pps.size()) {
      throw std::runtime_error("Transcryptor returned wrong number of entries");
    }

    ctx->ticket.mPseudonyms = std::move(resp.mEntries);
    if (ctx->ticket.mUserGroup == UserGroup::DataAdministrator && !ctx->ticket.mPseudonyms.empty()) {
      LOG(LOG_TAG, info) << "Granting " << ctx->ticket.mUserGroup << " unchecked access to " << ctx->ticket.mPseudonyms.size() << " participant(s)";
    }
    for (size_t i = 0; i < ctx->ticket.mPseudonyms.size(); i++) {
      LocalPseudonym localPseudonym = ctx->ticket.mPseudonyms[i].mAccessManager.decrypt(ctx->server->mPseudonymKey);
      if (ctx->ticket.mUserGroup != UserGroup::DataAdministrator) {
        ctx->server->backend->assertParticipantAccess(ctx->ticket.mUserGroup, localPseudonym, ctx->participantModes, ctx->ticket.mTimestamp);
      }
      if (ctx->pps[i].isClientProvided && !ctx->server->backend->hasLocalPseudonym(localPseudonym)) {
        if (ctx->ticket.hasMode("write")) {
          ctx->server->backend->storeLocalPseudonymAndPP(localPseudonym, ctx->ticket.mPseudonyms[i].mPolymorphic);
        }
      }
    }

    // All seems fine: finally, we log the ticket at the transcryptor
    ctx->signedTicket = SignedTicket2(std::move(ctx->ticket), *ctx->server->getSigningIdentity());

    LogIssuedTicketRequest logReq;
    logReq.mTicket = ctx->signedTicket;
    logReq.mId = resp.mId;
    LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " logging issued ticket";
    return ctx->server->mTranscryptorProxy.requestLogIssuedTicket(std::move(logReq));
  }).map([ctx](LogIssuedTicketResponse resp) {
    LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " finishing up";
    ctx->signedTicket.mTranscryptorSignature = std::move(resp.mSignature);

    std::string response;
    if (!ctx->request.mRequestIndexedTicket) {
      response = Serialization::ToString(std::move(ctx->signedTicket));
    }
    else {
      response = Serialization::ToString(
        IndexedTicket2(std::make_shared<SignedTicket2>(
          std::move(ctx->signedTicket)),
          std::move(ctx->columnGroupMap), std::move(ctx->participantGroupMap)));
    }
    auto result = rxcpp::observable<>::from(MakeSharedCopy(std::move(response))).as_dynamic();

    ctx->server->lpMetrics->ticket_request2_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - ctx->start_time).count());
    LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " returning ticket to requestor";
    return result;
  });

  result = rxcpp::observable<>::empty<messaging::MessageSequence>()
    .tap(
      [](auto) { /*ignore */},
      [](std::exception_ptr) { /*ignore */},
      [ctx]() {
        LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " starting asynchronous processing";
      })
    .concat(result);

  LOG(LOG_TAG, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << requestNumber << " returning observable";
  return result;
}

messaging::MessageBatches AccessManager::handleAmaMutationRequest(std::shared_ptr<SignedAmaMutationRequest> signedRequest) {
  AmaMutationRequest request = signedRequest->open(*this->getRootCAs());
  std::string userGroup = signedRequest->getLeafCertificateOrganizationalUnit();
  backend->performMutationsForRequest(request, userGroup);

  // Perform the adding of participants operations (and return 0..n FakeVoid items)
  return this->addParticipantsToGroupsForRequest(request)
    // Perform the removal of participants operations (and return 0..n FakeVoid items)
    .concat(this->removeParticipantsFromGroupsForRequest(request))
    // Ignore earlier items and just return a _single_ FakeVoid, so that we know that the concat_map (below) is invoked only once
    .op(RxInstead(FakeVoid()))
    // Return a (single) AmaMutationResponse (serialized and converted to obs<obs<shared>>)
    .concat_map([](FakeVoid) {return messaging::BatchSingleMessage(AmaMutationResponse()); });
}


rxcpp::observable<FakeVoid> AccessManager::addParticipantsToGroupsForRequest(const AmaMutationRequest& amRequest) {
  return this->removeOrAddParticipantsInGroupsForRequest(amRequest, false);
}
rxcpp::observable<FakeVoid> AccessManager::removeParticipantsFromGroupsForRequest(const AmaMutationRequest& amRequest) {
  return this->removeOrAddParticipantsInGroupsForRequest(amRequest, true);
}

rxcpp::observable<FakeVoid> AccessManager::removeOrAddParticipantsInGroupsForRequest(const AmaMutationRequest& amRequest, const bool performRemove) {
  std::map<std::string, std::vector<PolymorphicPseudonym>> participantsMap;
  // This method is used for adding and removing participants. The behaviour is defined by the 'performRemove' bool. When false, participant is added and vise versa.
  if (performRemove)
    for (auto& x : amRequest.mRemoveParticipantFromGroup)
      participantsMap[x.mParticipantGroup].push_back(x.mParticipant);
  else
    for (auto& x : amRequest.mAddParticipantToGroup)
      participantsMap[x.mParticipantGroup].push_back(x.mParticipant);

  return RxIterate(participantsMap)
      .concat_map([self = SharedFrom(*this), performRemove](const std::pair<const std::string, std::vector<PolymorphicPseudonym>>& entry) {
    auto& [participantGroup, list] = entry;
    TicketRequest2 ticketRequest;
    ticketRequest.mParticipantGroups = {participantGroup};
    ticketRequest.mModes = {"enumerate"};
    ticketRequest.mPolymorphicPseudonyms = list;
    TranscryptorRequest tsRequest;
    std::string data = Serialization::ToString(ticketRequest);
    tsRequest.mRequest = SignedTicketRequest2(std::nullopt, Signature::Make(data, *self->getSigningIdentity(), true), data);
    TranscryptorRequestEntries tsRequestEntries;
    tsRequestEntries.mEntries.resize(list.size());  // TODO: chunk according to TS_REQUEST_BATCH_SIZE
    for (size_t i = 0; i < list.size(); i++) {
      TranscryptorRequestEntry& entry = tsRequestEntries.mEntries[i];
      entry.mPolymorphic = list[i];
          FillTranscryptorRequestEntry(entry, *self->mPseudonymTranslator);
    }
    return self->mTranscryptorProxy.requestTranscryption(std::move(tsRequest), messaging::MakeSingletonTail(tsRequestEntries))
      .map([server = SharedFrom(*self), participantGroup = participantGroup, performRemove](TranscryptorResponse resp) -> FakeVoid {
              LocalPseudonym localPseudonym = resp.mEntries[0].mAccessManager.decrypt(server->mPseudonymKey);
             if (performRemove)
               server->backend->removeParticipantFromGroup(localPseudonym, participantGroup);
             else
               server->backend->addParticipantToGroup(localPseudonym, participantGroup);
             return FakeVoid();
           });
                });
}


std::vector<AmaQueryResponse> AccessManager::ExtractPartialColumnGroupQueryResponse(const std::vector<AmaQRColumnGroup>& columnGroups, const size_t maxSize) {
  std::vector<AmaQueryResponse> responses;
  if (columnGroups.size() > 0) {
    responses.emplace_back();
  }
  size_t responseSize{0U};
  const size_t limitedMessageSize = static_cast<size_t>(0.9 * static_cast<double>(maxSize)); // allow for some padding by serialisation.

  auto sourceColumnGroup = columnGroups.cbegin();
  size_t firstColumn = 0U;
  while (sourceColumnGroup != columnGroups.cend()) {
    auto& currentResponse = responses.back();
    size_t sizeLeft = limitedMessageSize > responseSize ? limitedMessageSize - responseSize : 0; // guard against underflowing

    auto destinationColumnGroup = AmaQRColumnGroup();
    auto entrySize = AmaQRColumnGroup::FillToProtobufSerializationCapacity(destinationColumnGroup, *sourceColumnGroup, sizeLeft, firstColumn);

    // Only if columns were added to the entry OR the source columngroup is empty itself, add it to the response. Otherwise, put it in the next.
    if (entrySize != 0 && (destinationColumnGroup.mColumns.size() != 0 || sourceColumnGroup->mColumns.size() == 0)) {
      currentResponse.mColumnGroups.push_back(destinationColumnGroup);
      firstColumn += destinationColumnGroup.mColumns.size();
      responseSize += entrySize;
    }
    else {
      if (responseSize == 0U) {
        // The response is empty, but a new response is prompted. This will lead to an infinite loop.
        throw std::runtime_error("Processing column group " + sourceColumnGroup->mName + ", a new AmaQueryResponse was prompted while the last response was still empty. Is the maxSize set correctly? maxSize: " + std::to_string(maxSize));
      }

      responses.emplace_back();
      responseSize = 0U;
    }

    if (firstColumn == sourceColumnGroup->mColumns.size()) {
      firstColumn = 0U;
      sourceColumnGroup++;
    }

  }
  return responses;
}



messaging::MessageBatches
AccessManager::handleAmaQuery(std::shared_ptr<SignedAmaQuery> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  auto userGroup = signedRequest->getLeafCertificateOrganizationalUnit();

  auto resp = backend->performAMAQuery(request, userGroup);

  // Split information over multiple responses to keep message size down. See #1679.
  return RxIterate(ExtractPartialQueryResponse(resp, &AmaQueryResponse::mColumns))
    .concat(RxIterate(ExtractPartialColumnGroupQueryResponse(resp.mColumnGroups)))
    .concat(RxIterate(ExtractPartialQueryResponse(resp, &AmaQueryResponse::mColumnGroupAccessRules)))
    .concat(RxIterate(ExtractPartialQueryResponse(resp, &AmaQueryResponse::mParticipantGroups)))
    .concat(RxIterate(ExtractPartialQueryResponse(resp, &AmaQueryResponse::mParticipantGroupAccessRules)))
    .map([](const AmaQueryResponse& response) {
    return rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(response))).as_dynamic();
         });
}

messaging::MessageBatches AccessManager::handleUserQuery(std::shared_ptr<SignedUserQuery> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  auto accessGroup = signedRequest->getLeafCertificateOrganizationalUnit();

  return messaging::BatchSingleMessage(backend->performUserQuery(request, accessGroup));
}

messaging::MessageBatches AccessManager::handleUserMutationRequest(std::shared_ptr<SignedUserMutationRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  auto accessGroup = signedRequest->getLeafCertificateOrganizationalUnit();

  return backend->performUserMutationsForRequest(request, accessGroup).map([](UserMutationResponse response) -> messaging::MessageSequence {
    return rxcpp::rxs::just(std::make_shared<std::string>(Serialization::ToString(response)));
  });
}

messaging::MessageBatches AccessManager::handleVerifiersRequest(std::shared_ptr<VerifiersRequest> request) {
  return messaging::BatchSingleMessage(VerifiersResponse(
      mPseudonymTranslator->computeTranslationProofVerifiers(
          RecipientForServer(EnrolledParty::AccessManager),
          mPublicKeyPseudonyms
      ),
      mPseudonymTranslator->computeTranslationProofVerifiers(
          RecipientForServer(EnrolledParty::StorageFacility),
          mPublicKeyPseudonyms
      ),
      mPseudonymTranslator->computeTranslationProofVerifiers(
          RecipientForServer(EnrolledParty::Transcryptor),
          mPublicKeyPseudonyms
      )
  ));
}

messaging::MessageBatches
AccessManager::handleColumnAccessRequest(
  std::shared_ptr<SignedColumnAccessRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  auto userGroup = signedRequest->getLeafCertificateOrganizationalUnit();

  return messaging::BatchSingleMessage(backend->handleColumnAccessRequest(request, userGroup));
}

messaging::MessageBatches
AccessManager::handleParticipantGroupAccessRequest(
  std::shared_ptr<SignedParticipantGroupAccessRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  auto userGroup = signedRequest->getLeafCertificateOrganizationalUnit();

  return messaging::BatchSingleMessage(backend->handleParticipantGroupAccessRequest(request, userGroup));
}

messaging::MessageBatches AccessManager::handleColumnNameMappingRequest
(std::shared_ptr<SignedColumnNameMappingRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  auto userGroup = signedRequest->getLeafCertificateOrganizationalUnit();

  return messaging::BatchSingleMessage(backend->handleColumnNameMappingRequest(request, userGroup));
}

messaging::MessageBatches AccessManager::
    handleMigrateUserDbToAccessManagerRequest(std::shared_ptr<SignedMigrateUserDbToAccessManagerRequest> signedRequest, messaging::MessageSequence chunksObservable) {
  UserGroup::EnsureAccess(UserGroup::Authserver, signedRequest->getLeafCertificateOrganizationalUnit());
  signedRequest->validate(*this->getRootCAs()); //The request itself is empty, but we do want to check the signature
  assert(getStoragePath().has_value());
  backend->ensureNoUserData();
  auto tmpUserDbMigrationPath = getStoragePath().value() / pep::filesystem::RandomizedName("AuthserverStorage-%%%%%%.sqlite");
  LOG(LOG_TAG, info) << "Received MigrateUserDbToAccessManagerRequest. Storing authserver storage as " << tmpUserDbMigrationPath;

  return chunksObservable.reduce(
    MakeStreamWithDeferredCleanup(tmpUserDbMigrationPath),
    [](auto streamWithCleanup, std::shared_ptr<std::string> chunk) {
      *streamWithCleanup.first << *chunk;
      return streamWithCleanup;
    },
    [tmpUserDbMigrationPath, backend=this->backend](auto streamWithCleanup) -> messaging::MessageSequence {
      streamWithCleanup.first->close();
      return rxcpp::rxs::just(std::make_shared<std::string>(Serialization::ToString(backend->migrateUserDb(tmpUserDbMigrationPath))));
    }
  );
}

messaging::MessageBatches AccessManager::handleFindUserRequest(
    std::shared_ptr<SignedFindUserRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  auto userGroup = signedRequest->getLeafCertificateOrganizationalUnit();

  return messaging::BatchSingleMessage(backend->handleFindUserRequest(request, userGroup));
}

messaging::MessageBatches AccessManager::handleStructureMetadataRequest(std::shared_ptr<SignedStructureMetadataRequest> signedRequest) {
  auto request = signedRequest->open(*this->getRootCAs());
  auto userGroup = signedRequest->getLeafCertificateOrganizationalUnit();

  auto entries = backend->handleStructureMetadataRequest(request, userGroup);
  return
      RxIterate(std::move(entries))
      .map([](StructureMetadataEntry entry) {
        return rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(std::move(entry))))
            .as_dynamic();
      });
}

messaging::MessageBatches AccessManager::handleSetStructureMetadataRequest(
    std::shared_ptr<SignedSetStructureMetadataRequest> signedRequest,
    messaging::MessageSequence chunks) {
  auto request = signedRequest->open(*this->getRootCAs());
  auto userGroup = signedRequest->getLeafCertificateOrganizationalUnit();

  backend->handleSetStructureMetadataRequestHead(request, userGroup);

  return
      chunks.map([backend = backend, subjectType = request.subjectType, userGroup
        ](const std::shared_ptr<std::string>& chunk) {
          StructureMetadataEntry entry = Serialization::FromString<StructureMetadataEntry>(*chunk);

          backend->handleSetStructureMetadataRequestEntry(subjectType, entry, userGroup);
          return FakeVoid{};
        })
      .op(RxInstead(
        rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(SetStructureMetadataResponse{})))
        .as_dynamic()));
}

} // namespace pep
