#include <pep/accessmanager/AccessManager.hpp>
#include <pep/accessmanager/Backend.hpp>

#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/accessmanager/AmaSerializers.hpp>
#include <pep/async/RxBeforeCompletion.hpp>
#include <pep/async/RxIndexed.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/auth/EnrolledParty.hpp>
#include <pep/auth/UserGroup.hpp>
#include <pep/morphing/MorphingPropertySerializers.hpp>
#include <pep/morphing/RepoRecipient.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/rsk/RskSerializers.hpp>
#include <pep/structure/ColumnNameSerializers.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Filesystem.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/accessmanager/UserSerializers.hpp>

#include <numeric>
#include <ranges>
#include <sstream>
#include <chrono>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-buffer_count.hpp>
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
  std::tie(entry.accessManager, entry.accessManagerProof) =
      pseudonymTranslator.certifiedTranslateStep(
          entry.polymorphic,
          RecipientForServer(EnrolledParty::AccessManager));
  std::tie(entry.storageFacility, entry.storageFacilityProof) =
      pseudonymTranslator.certifiedTranslateStep(
          entry.polymorphic,
          RecipientForServer(EnrolledParty::StorageFacility));
  std::tie(entry.transcryptor, entry.transcryptorProof) =
      pseudonymTranslator.certifiedTranslateStep(
          entry.polymorphic,
          RecipientForServer(EnrolledParty::Transcryptor));
  entry.ensurePacked();
}

const std::string LogTag ("AccessManager");
const Severity TICKET_REQUEST_LOGGING_SEVERITY = Severity::Debug;

constexpr size_t TS_REQUEST_BATCH_SIZE = 400U;

const size_t MAX_AMA_QUERY_RESPONSE_STRINGS = 25000; // See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2089#note_25719
const std::size_t AMA_QUERY_RESPONSE_STRINGS_WARNING_THRESHOLD = static_cast<std::size_t>(0.8 * MAX_AMA_QUERY_RESPONSE_STRINGS);

void FillTranscryptorRequestEntry(
    TranscryptorRequestEntry& entry,
    const PseudonymTranslator& pseudonymTranslator,
    const std::optional<PseudonymTranslator::Recipient>& userRecipient
) {
  if (userRecipient) {
    std::tie(entry.userGroup.emplace(), entry.userGroupProof.emplace()) =
        pseudonymTranslator.certifiedTranslateStep(
            entry.polymorphic,
            *userRecipient);
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
      PEP_LOG(LogTag, Severity::Warning) << "(Excessively) large AMA query response entry: " << NormalizedTypeNamer<T>::GetTypeName() << " contains " + std::to_string(entryStrings) + " strings";
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
  auto deferredCleanup = DeferShared([stream, path]() {
    stream->close();
    std::filesystem::remove(path);
  });
  return std::make_pair(stream, deferredCleanup);
}

} // namespace

AccessManager::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry) :
  RegisteredMetrics(registry),
  enckeyRequestDuration(prometheus::BuildSummary()
    .Name("pep_accessmanager_enckey_request_duration_seconds")
    .Help("Duration of a successful encryptionkey request")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}, std::chrono::minutes{5})),
  ticketRequest2Duration(prometheus::BuildSummary()
    .Name("pep_accessmanager_ticket_request2_duration_seconds")
    .Help("Duration of a successful ticket2 request")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}, std::chrono::minutes{5})),
  ticketRequestDuration(prometheus::BuildSummary()
    .Name("pep_accessmanager_ticket_request_duration_seconds")
    .Help("Duration of a successful ticket request")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}, std::chrono::minutes{5})) {
}


AccessManager::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : KeyComponentServer::Parameters(io_context, config) {
  std::filesystem::path keysFile;
  std::filesystem::path globalConfFile;

  std::filesystem::path storageFile;

  try {
    keysFile = config.get<std::filesystem::path>("EnrolledPartyKeysFile");
    globalConfFile = config.get<std::filesystem::path>("GlobalConfigurationFile");

    auto serverEndPoints = config.get_child("ServerEndPoints");
    transcryptorEndPoint_ = serverEndPoints.get<EndPoint>(ServerTraits::Transcryptor().configNode());
    keyServerEndPoint_ = serverEndPoints.get<EndPoint>(ServerTraits::KeyServer().configNode());

    storageFile = config.get<std::filesystem::path>("StorageFile");
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with configuration file: " << e.what();
    throw;
  }

  try {
    auto enrolledPartyKeys = Configuration::FromFile(keysFile).get<EnrolledPartyKeys>("");
    setPseudonymKey(enrolledPartyKeys.pseudonymKey.value());
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with keys file: " << keysFile << " : " << e.what();
    throw;
  }

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
  this->globalConf_ = gc;
}

std::shared_ptr<GlobalConfiguration> AccessManager::Parameters::getGlobalConfiguration() const {
  return globalConf_;
}

/*!
  * \return The pseudonym key
  */
const ElgamalPrivateKey& AccessManager::Parameters::getPseudonymKey() const {
  return pseudonymKey_.value();
}

/*!
  * \param pseudonymKey The pseudonym key
  */
void AccessManager::Parameters::setPseudonymKey(const ElgamalPrivateKey& pseudonymKey) {
  pseudonymKey_ = pseudonymKey;
}

const EndPoint& AccessManager::Parameters::getTranscryptorEndPoint() const {
  return transcryptorEndPoint_;
}

const EndPoint& AccessManager::Parameters::getKeyServerEndPoint() const {
  return keyServerEndPoint_;
}

std::shared_ptr<AccessManager::Backend> AccessManager::Parameters::getBackend() const {
  return backend_;
}
void AccessManager::Parameters::setBackend(std::shared_ptr<AccessManager::Backend> backend) {
  backend_ = backend;
}

void AccessManager::Parameters::check() const {
  if (!pseudonymKey_)
    throw std::runtime_error("pseudonymKey must be set");
  if (!backend_)
    throw std::runtime_error("backend must be set");

  KeyComponentServer::Parameters::check();
}

AccessManager::AccessManager(std::shared_ptr<AccessManager::Parameters> parameters)
  : KeyComponentServer(parameters),
  pseudonymKey_(parameters->getPseudonymKey()),
  transcryptorProxy_(messaging::ServerConnection::Create(this->getIoContext(), parameters->getTranscryptorEndPoint(), parameters->getRootCACertificatesFilePath()), *this, parameters->getTranscryptorEndPoint().expectedCommonName, getRootCAs()),
  keyServerProxy_(messaging::ServerConnection::Create(this->getIoContext(), parameters->getKeyServerEndPoint(), parameters->getRootCACertificatesFilePath()), *this),
  backend_(parameters->getBackend()),
  globalConf_(parameters->getGlobalConfiguration()),
  lpMetrics_(std::make_shared<AccessManager::Metrics>(registry_)),
  workerPool_(WorkerPool::getShared()) {
  backend_->setAccessManager(this);
  RegisterRequestHandlers(*this,
                          &AccessManager::handleTicketRequest2,
                          &AccessManager::handleEncryptionKeyRequest,
                          &AccessManager::handleGlobalConfigurationRequest,
                          &AccessManager::handleAmaMutationRequest,
                          &AccessManager::handleAmaQuery,
                          &AccessManager::handleUserQuery,
                          &AccessManager::handleUserMutationRequest,
                          &AccessManager::handleVerifiersRequest,
                          &AccessManager::handleUserVerifiersRequest,
                          &AccessManager::handleColumnAccessRequest,
                          &AccessManager::handleParticipantGroupAccessRequest,
                          &AccessManager::handleColumnNameMappingRequest,
                          &AccessManager::handleFindUserRequest,
                          &AccessManager::handleMigrateUserDbToAccessManagerRequest,
                          &AccessManager::handleStructureMetadataRequest,
                          &AccessManager::handleSetStructureMetadataRequest);
}

std::optional<std::filesystem::path> AccessManager::getStoragePath() {
  return EnsureDirectoryPath(backend_->getStoragePath().parent_path());
}

std::unordered_set<std::string> AccessManager::getAllowedChecksumChainRequesters() {
  auto result = Server::getAllowedChecksumChainRequesters();
  for (auto& authserver : UserGroup::Authserver) result.insert(authserver);
  return result;
}

messaging::MessageBatches AccessManager::handleGlobalConfigurationRequest(std::shared_ptr<GlobalConfigurationRequest> request) {
  return messaging::BatchSingleMessage(*globalConf_);
}

messaging::MessageBatches
AccessManager::handleEncryptionKeyRequest(std::shared_ptr<SignedEncryptionKeyRequest> signedRequest) {
  auto start_time = std::chrono::steady_clock::now();
  auto certified = signedRequest->open(*this->getRootCAs());
  auto userGroup = certified.signatory.organizationalUnit();
  auto request = std::make_shared<EncryptionKeyRequest>(std::move(certified.message));
  auto clientCertificateChain = MakeSharedCopy(certified.signatory.certificateChain());
  const auto party = GetEnrolledParty(clientCertificateChain->leaf());
  if (!party.has_value()) {
    throw std::runtime_error("Cannot produce encryption key for this requestor");
  }
  if (!HasDataAccess(*party)) {
    throw std::runtime_error("Unsupported enrolled party " + std::to_string(static_cast<unsigned>(*party)));
  }

  const auto recipient = std::make_shared<RekeyRecipient>(
    RekeyRecipientForCertificate(clientCertificateChain->leaf()));

  if (request->ticket2 == nullptr)
    throw Error("Invalid signature or missing ticket");

  auto ticket = request->ticket2->open(*this->getRootCAs(), userGroup);

  backend_->checkTicketForEncryptionKeyRequest(request, ticket);

  // Note that it is clear that the client has access to the given
  // participants as their polymorphic pseudonyms are taken from the
  // list in the signed ticket.

  auto lpResponse = std::make_shared<EncryptionKeyResponse>();
  lpResponse->keys.resize(request->entries.size());


  size_t dwNumUnblind = 0;
  for (const auto& entry : request->entries)
    if (entry.keyBlindMode == KeyBlindMode::Unblind)
      dwNumUnblind++;

  // Decrypt local pseudonyms
  auto server = SharedFrom(*this);
  auto localPseudonyms = std::make_shared<std::vector<LocalPseudonym>>();
  return server->workerPool_->batched_map<8>(ticket.accessSubjects,
        ObserveOnAsio(*server->getIoContext()),
        [server, localPseudonyms](LocalPseudonyms elp) -> LocalPseudonym {
          return elp.accessManager.decrypt(server->pseudonymKey_);
        })
      .flat_map([start_time, server, dwNumUnblind, lpResponse, request, clientCertificateChain, recipient, localPseudonyms
        ](std::vector<LocalPseudonym> localPseudonymsOnStack) {
          *localPseudonyms = std::move(localPseudonymsOnStack);
          return server->workerPool_->batched_map<8>(request->entries,
                ObserveOnAsio(*server->getIoContext()),
                [server, localPseudonyms](KeyRequestEntry entry) {
                  EncryptedKey key;

                  if (entry.keyBlindMode == KeyBlindMode::Blind) {
                    LocalPseudonym localPseudonym_;
                    try {
                      localPseudonym_ = localPseudonyms->at(entry.pseudonymIndex);
                    } catch (const std::out_of_range&) {
                      PEP_LOG(LogTag, Severity::Critical) << "Out of bounds read on local pseudonyms vector during key blinding";
                      throw;
                    }
                    auto blindingAD = entry.metadata.computeKeyBlindingAdditionalData(localPseudonym_);
                    key = server->dataTranslator().blind(
                      entry.polymorphEncryptionKey,
                      blindingAD.content,
                      blindingAD.invertComponent
                    );
                    key.ensurePacked();
                  } else if (entry.keyBlindMode == KeyBlindMode::Unblind) {
                    // do nothing --- we need the transcryptor to help out
                  } else {
                    std::ostringstream msg;
                    msg << "Received unknown blinding mode: " << ToUnderlying(entry.keyBlindMode);
                    throw Error(msg.str());
                  }
                  return key;
                })
              .flat_map([start_time, server, dwNumUnblind, lpResponse, request, clientCertificateChain, recipient, localPseudonyms
                ](std::vector<EncryptedKey> keys) -> messaging::MessageBatches {
                  lpResponse->keys = std::move(keys);

                  if (dwNumUnblind == 0) {
                    server->lpMetrics_->enckeyRequestDuration.Observe(
                      std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count());
                    return messaging::BatchSingleMessage(*lpResponse);
                  }

                  /* if we find at least one unblind entry in the request
                    * we can't deal with this ourselves, we need the transcryptor for this
                    */
                  PEP_LOG(LogTag, Severity::Debug) << "Rekey request has a KeyBlindMode::Unblind entry -> forwarding to transcryptor";
                  RekeyRequest rkReq{
                    .keys{},
                    .clientCertificateChain = *clientCertificateChain,
                  };
                  rkReq.keys.reserve(dwNumUnblind);

                  // Index of the entry into Rekey{Request,Response}.
                  auto rkIndices = std::make_shared<std::vector<uint32_t>>(request->entries.size());

                  for (size_t i = 0; i < request->entries.size(); i++) {
                    auto& entry = request->entries[i];
                    if (entry.keyBlindMode != KeyBlindMode::Unblind)
                      continue;
                    try {
                      rkIndices->at(i) = static_cast<uint32_t>(rkReq.keys.size());
                    } catch (const std::out_of_range&) {
                      PEP_LOG(LogTag, Severity::Critical) << "Out of bounds read on rekey indices vector during key unblinding";
                      throw;
                    }
                    rkReq.keys.push_back(entry.polymorphEncryptionKey);
                  }

                  return server->transcryptorProxy_.requestRekey(std::move(rkReq)).flat_map(
                    [server, rkIndices, start_time, request, recipient, lpResponse, localPseudonyms
                    ](RekeyResponse transRespOnStack) {

                      auto transResp = std::make_shared<RekeyResponse>(std::move(transRespOnStack));

                      // workerPool_->batched_map() does not tell us which index we're handling,
                      // so we let it process indices to work around this.  If we need this
                      // more often, it's better to change batched_map()
                      std::vector<size_t> is(request->entries.size());
                      std::iota(is.begin(), is.end(), 0);
                      return server->workerPool_->batched_map<8>(is,
                            ObserveOnAsio(*server->getIoContext()),
                            [server, request, lpResponse, transResp, rkIndices, localPseudonyms, recipient
                            ](size_t i) {
                              auto& entry = request->entries[i];
                              auto& key = lpResponse->keys[i];

                              if (entry.keyBlindMode != KeyBlindMode::Unblind)
                                return i; // we have to return something

                              //TODO: check access once access is based on local pseudonyms
                              LocalPseudonym lp;
                              try {
                                lp = localPseudonyms->at(entry.pseudonymIndex);
                              } catch (const std::out_of_range&) {
                                PEP_LOG(LogTag, Severity::Critical) << "Out of bounds read on local pseudonyms vector during key unblinding";
                                throw;
                              }
                              auto blindingAD = entry.metadata.computeKeyBlindingAdditionalData(lp);

                              EncryptedKey encryptedKey;
                              try {
                                encryptedKey = transResp->keys.at((*rkIndices)[i]);
                              } catch (const std::out_of_range&) {
                                PEP_LOG(LogTag, Severity::Critical) << "Out of bounds read on keys vector during unblinding-and-rekeying";
                                throw;
                              }

                              key = server->dataTranslator().unblindAndTranslate(
                                encryptedKey,
                                blindingAD.content,
                                blindingAD.invertComponent,
                                *recipient);
                              key.ensurePacked();
                              return i; // we have to return something
                            })
                          .map([server, lpResponse, start_time](std::vector<size_t>) {
                            server->lpMetrics_->enckeyRequestDuration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count());
                            return rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(*lpResponse))).as_dynamic();
                          });
                    });
                });
        });
}

std::vector<std::string> AccessManager::getChecksumChainNames() const {
  return backend_->getChecksumChainNames();
}

void AccessManager::computeChecksumChainChecksum(
  const std::string& chain,
  std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
  uint64_t& checkpoint) {
  backend_->computeChecksum(chain, maxCheckpoint, checksum, checkpoint);
}

messaging::MessageBatches
AccessManager::handleTicketRequest2(std::shared_ptr<SignedTicketRequest2> signedRequest) {
  using namespace std::ranges;

  auto time = std::chrono::steady_clock::now();
  auto requestNumber = nextTicketRequestNumber_++;

  PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << requestNumber << " received";

  // openAsAccessManager checks that signature_ and logSignature_ are set,
  // are valid and match.
  auto certified = signedRequest->openAsAccessManager(*this->getRootCAs());
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  backend_->checkTicketRequest(request);

  auto timestamp = TimeNow();

  auto pps = RangeToVector(request.accessSubjects
    | views::transform([](const PolymorphicPseudonym& pp) { return Backend::Pp{pp, true}; }));

  std::vector<std::string> modes{"access"};
  std::unordered_map<std::string, IndexList> participantGroupMap;
  if (!request.participantGroups.empty()) {
    // Access to participants does not imply permission to list groups they are in, so first check that
    backend_->checkParticipantGroupAccess(request.participantGroups, userGroup, modes, timestamp);

    participantGroupMap = backend_->fillParticipantGroupMap(request.participantGroups, pps);
  }

  // Prepare ticket

  Ticket2 ticket;
  ticket.timestamp = TimeNow();
  ticket.modes = request.modes;
  ticket.columns = request.columns;
  ticket.userGroup = userGroup;

  // Check columns and column groups
  auto columnGroupMap = backend_->unfoldColumnGroupsAndCheckAccess(
      userGroup, request.columnGroups, request.modes, timestamp, ticket.columns /*in & out*/);

  // Remove the main client signature to prevent reuse of
  // the SignedTicketRequest2.
  auto signature = signedRequest->extractSignature();

  // Because of all the asynchronous IO, we move all state into this context
  // struct, so that we don't have to put everything into shared_ptrs
  struct Context {
    std::shared_ptr<AccessManager> server;
    uintmax_t requestNumber;
    TicketRequest2 request;
    Ticket2 ticket;
    SignedTicket2 signedTicket{};
    std::vector<Backend::Pp> pps;
    decltype(time) start_time;
    std::unordered_map<std::string, IndexList> columnGroupMap;
    std::unordered_map<std::string, IndexList> participantGroupMap;
    std::vector<std::string> participantModes;
    TranscryptorRequest tsReq;
    TranscryptorRequestEntries tsReqEntries{};
    std::optional<PseudonymTranslator::Recipient> userRecipient;
  };

  auto userRecipient = request.includeUserGroupPseudonyms
    ? std::optional{RecipientForCertificate(signature.certificateChain().leaf())}
    : std::nullopt;

  auto ctx = MakeSharedCopy(Context{
    .server = SharedFrom(*this),
    .requestNumber = requestNumber,
    .request = request,
    .ticket = std::move(ticket),
    .pps = std::move(pps),
    .start_time = time,
    .columnGroupMap = std::move(columnGroupMap),
    .participantGroupMap = std::move(participantGroupMap),
    .participantModes = std::move(modes),
    .tsReq {.request = std::move(*signedRequest) },
    .userRecipient = std::move(userRecipient),
    });

  // Prepare transcryptor request
  ctx->tsReqEntries.entries.resize(ctx->pps.size());

  PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << requestNumber << " constructing observable";

  // workerPool_->batched_map() does not tell us which index we're handling,
  // so we let it process indices to work around this.  If we need this
  // more often, it's better to change batched_map()
  auto indexes = RangeToVector(views::iota(std::size_t{}, ctx->pps.size()));
  messaging::MessageBatches result =
    ctx->server->workerPool_->batched_map<8>(std::move(indexes),
        ObserveOnAsio(*ctx->server->getIoContext()),
      [ctx](size_t i) {
    const Backend::Pp& pp = ctx->pps[i];
    TranscryptorRequestEntry& entry = ctx->tsReqEntries.entries[i];

    // Rerandomize old PPs (ie. from the database)
    // To prevent multiple users receiving identical PPs
    if (pp.isClientProvided)
      entry.polymorphic = pp.pp;
    else
      entry.polymorphic = pp.pp.rerandomize();

    FillTranscryptorRequestEntry(
        entry,
        ctx->server->pseudonymTranslator(),
        ctx->userRecipient);
    return i;
  }).flat_map([ctx](std::vector<size_t> is) {
    // Send request to transcryptor

    auto numEntries = ctx->tsReqEntries.entries.size();
    auto tail = RxIterate(std::move(ctx->tsReqEntries.entries))
      .buffer(static_cast<int>(TS_REQUEST_BATCH_SIZE))
      .as_dynamic() // Reduce compiler memory usage
      .op(RxIndexed<std::uint32_t>())
      .map([requestNumber = ctx->requestNumber](std::pair<std::uint32_t, std::vector<TranscryptorRequestEntry>> pair) {
        auto& [batchNum, batch] = pair;
        PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << requestNumber << " sending transcryptor request entry batch " << batchNum << " containing " << batch.size() << " entries";
        return messaging::MakeTailSegment(TranscryptorRequestEntries{std::move(batch)});
      })
      .op(RxBeforeCompletion([requestNumber = ctx->requestNumber, numEntries] {
        PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << requestNumber << " sent " << numEntries << " transcryptor request entries";
      }));

    PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " sending transcryptor request";
    return ctx->server->transcryptorProxy_.requestTranscryption(ctx->tsReq, tail);
  }).flat_map([ctx](TranscryptorResponse resp) {
    PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " received transcryptor response";
    // Now we have local pseudonyms for the original PPs.
    if (resp.entries.size() != ctx->pps.size()) {
      throw std::runtime_error("Transcryptor returned wrong number of entries");
    }

    ctx->ticket.accessSubjects = std::move(resp.entries);
    if (ctx->ticket.userGroup == UserGroup::DataAdministrator && !ctx->ticket.accessSubjects.empty()) {
      PEP_LOG(LogTag, Severity::Info) << "Granting " << ctx->ticket.userGroup << " unchecked access to " << ctx->ticket.accessSubjects.size() << " participant(s)";
    }
    for (size_t i = 0; i < ctx->ticket.accessSubjects.size(); i++) {
      LocalPseudonym localPseudonym = ctx->ticket.accessSubjects[i].accessManager.decrypt(ctx->server->pseudonymKey_);
      if (ctx->ticket.userGroup != UserGroup::DataAdministrator) {
        ctx->server->backend_->checkParticipantAccess(ctx->ticket.userGroup, localPseudonym, ctx->participantModes, ctx->ticket.timestamp);
      }
      if (ctx->pps[i].isClientProvided && !ctx->server->backend_->hasLocalPseudonym(localPseudonym)) {
        if (ctx->ticket.hasMode("write")) {
          ctx->server->backend_->storeLocalPseudonymAndPP(localPseudonym, ctx->ticket.accessSubjects[i].polymorphic);
        }
      }
    }

    // All seems fine: finally, we log the ticket at the transcryptor
    ctx->signedTicket = SignedTicket2(std::move(ctx->ticket), *ctx->server->getSigningIdentity());

    LogIssuedTicketRequest logReq;
    logReq.ticket = ctx->signedTicket;
    logReq.id = resp.id;
    PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " logging issued ticket";
    return ctx->server->transcryptorProxy_.requestLogIssuedTicket(std::move(logReq));
  }).map([ctx](LogIssuedTicketResponse resp) {
    PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " finishing up";
    ctx->signedTicket.addTranscryptorSignature(std::move(resp.signature));

    std::string response;
    if (!ctx->request.requestIndexedTicket) {
      response = Serialization::ToString(std::move(ctx->signedTicket));
    }
    else {
      response = Serialization::ToString(
        IndexedTicket2(std::make_shared<SignedTicket2>(
          std::move(ctx->signedTicket)),
          std::move(ctx->columnGroupMap), std::move(ctx->participantGroupMap)));
    }
    auto result = rxcpp::observable<>::from(MakeSharedCopy(std::move(response))).as_dynamic();

    ctx->server->lpMetrics_->ticketRequest2Duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - ctx->start_time).count());
    PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " returning ticket to requestor";
    return result;
  });

  result = rxcpp::observable<>::empty<messaging::MessageSequence>()
    .tap(
      [](auto) { /*ignore */},
      [](std::exception_ptr) { /*ignore */},
      [ctx]() {
        PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << ctx->requestNumber << " starting asynchronous processing";
      })
    .concat(result);

  PEP_LOG(LogTag, TICKET_REQUEST_LOGGING_SEVERITY) << "Ticket request " << requestNumber << " returning observable";
  return result;
}

messaging::MessageBatches AccessManager::handleAmaMutationRequest(std::shared_ptr<SignedAmaMutationRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  std::string userGroup = certified.signatory.organizationalUnit();
  backend_->performMutationsForRequest(request, userGroup);

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
    for (auto& x : amRequest.removeParticipantFromGroup)
      participantsMap[x.participantGroup].push_back(x.participant);
  else
    for (auto& x : amRequest.addParticipantToGroup)
      participantsMap[x.participantGroup].push_back(x.participant);

  return RxIterate(participantsMap)
      .concat_map([self = SharedFrom(*this), performRemove](const std::pair<const std::string, std::vector<PolymorphicPseudonym>>& entry) {
    auto& [participantGroup, list] = entry;
    TicketRequest2 ticketRequest;
    ticketRequest.participantGroups = {participantGroup};
    ticketRequest.modes = {"enumerate"};
    ticketRequest.accessSubjects = list;
    std::string data = Serialization::ToString(ticketRequest);
    TranscryptorRequest tsRequest{
      .request = SignedTicketRequest2(std::nullopt, Signature::Make(data, *self->getSigningIdentity(), true), data)
    };
    TranscryptorRequestEntries tsRequestEntries;
    tsRequestEntries.entries.resize(list.size());  // TODO: chunk according to TS_REQUEST_BATCH_SIZE
    for (size_t i = 0; i < list.size(); i++) {
      TranscryptorRequestEntry& entry = tsRequestEntries.entries[i];
      entry.polymorphic = list[i];
          FillTranscryptorRequestEntry(entry, self->pseudonymTranslator());
    }
    return self->transcryptorProxy_.requestTranscryption(std::move(tsRequest), messaging::MakeSingletonTail(tsRequestEntries))
      .map([server = SharedFrom(*self), participantGroup, performRemove](const TranscryptorResponse& resp) -> FakeVoid {
        for (const LocalPseudonyms& pseudonyms : resp.entries) {
          LocalPseudonym localPseudonym = pseudonyms.accessManager.decrypt(server->pseudonymKey_);
          if (performRemove)
            server->backend_->removeParticipantFromGroup(localPseudonym, participantGroup);
          else
            server->backend_->addParticipantToGroup(localPseudonym, participantGroup);
        }
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
    if (entrySize != 0 && (destinationColumnGroup.columns.size() != 0 || sourceColumnGroup->columns.size() == 0)) {
      currentResponse.columnGroups.push_back(destinationColumnGroup);
      firstColumn += destinationColumnGroup.columns.size();
      responseSize += entrySize;
    }
    else {
      if (responseSize == 0U) {
        // The response is empty, but a new response is prompted. This will lead to an infinite loop.
        throw std::runtime_error("Processing column group " + sourceColumnGroup->name + ", a new AmaQueryResponse was prompted while the last response was still empty. Is the maxSize set correctly? maxSize: " + std::to_string(maxSize));
      }

      responses.emplace_back();
      responseSize = 0U;
    }

    if (firstColumn == sourceColumnGroup->columns.size()) {
      firstColumn = 0U;
      sourceColumnGroup++;
    }

  }
  return responses;
}



messaging::MessageBatches
AccessManager::handleAmaQuery(std::shared_ptr<SignedAmaQuery> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  auto resp = backend_->performAMAQuery(request, userGroup);

  // Split information over multiple responses to keep message size down. See #1679.
  return RxIterate(ExtractPartialQueryResponse(resp, &AmaQueryResponse::columns))
    .concat(RxIterate(ExtractPartialColumnGroupQueryResponse(resp.columnGroups)))
    .concat(RxIterate(ExtractPartialQueryResponse(resp, &AmaQueryResponse::columnGroupAccessRules)))
    .concat(RxIterate(ExtractPartialQueryResponse(resp, &AmaQueryResponse::participantGroups)))
    .concat(RxIterate(ExtractPartialQueryResponse(resp, &AmaQueryResponse::participantGroupAccessRules)))
    .map([](const AmaQueryResponse& response) {
    return rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(response))).as_dynamic();
         });
}

messaging::MessageBatches AccessManager::handleUserQuery(std::shared_ptr<SignedUserQuery> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto accessGroup = certified.signatory.organizationalUnit();

  return messaging::BatchSingleMessage(backend_->performUserQuery(request, accessGroup));
}

messaging::MessageBatches AccessManager::handleUserMutationRequest(std::shared_ptr<SignedUserMutationRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto accessGroup = certified.signatory.organizationalUnit();

  return backend_->performUserMutationsForRequest(request, accessGroup).map([](UserMutationResponse response) -> messaging::MessageSequence {
    return rxcpp::rxs::just(std::make_shared<std::string>(Serialization::ToString(response)));
  });
}

messaging::MessageBatches AccessManager::handleVerifiersRequest(std::shared_ptr<VerifiersRequest> request) {
  const auto& pseudonymTranslator = this->pseudonymTranslator();

  return messaging::BatchSingleMessage(VerifiersResponse{
      pseudonymTranslator.computeCertifiedTranslationProofVerifiers(
          RecipientForServer(EnrolledParty::AccessManager),
          systemPublicKeys().globalPseudonymEncryptionKey
      ),
      pseudonymTranslator.computeCertifiedTranslationProofVerifiers(
          RecipientForServer(EnrolledParty::StorageFacility),
          systemPublicKeys().globalPseudonymEncryptionKey
      ),
      pseudonymTranslator.computeCertifiedTranslationProofVerifiers(
          RecipientForServer(EnrolledParty::Transcryptor),
          systemPublicKeys().globalPseudonymEncryptionKey
      ),
  });
}

messaging::MessageBatches AccessManager::handleUserVerifiersRequest(std::shared_ptr<UserVerifiersRequest> request) {
  return messaging::BatchSingleMessage(UserVerifiersResponse{
    this->pseudonymTranslator().computeCertifiedTranslationProofVerifiers(
      RecipientForCertificate(request->userCertificate),
      systemPublicKeys().globalPseudonymEncryptionKey
    )
  });
}

messaging::MessageBatches
AccessManager::handleColumnAccessRequest(
  std::shared_ptr<SignedColumnAccessRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  return messaging::BatchSingleMessage(backend_->handleColumnAccessRequest(request, userGroup));
}

messaging::MessageBatches
AccessManager::handleParticipantGroupAccessRequest(
  std::shared_ptr<SignedParticipantGroupAccessRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  return messaging::BatchSingleMessage(backend_->handleParticipantGroupAccessRequest(request, userGroup));
}

messaging::MessageBatches AccessManager::handleColumnNameMappingRequest
(std::shared_ptr<SignedColumnNameMappingRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  return messaging::BatchSingleMessage(backend_->handleColumnNameMappingRequest(request, userGroup));
}

messaging::MessageBatches AccessManager::
    handleMigrateUserDbToAccessManagerRequest(std::shared_ptr<SignedMigrateUserDbToAccessManagerRequest> signedRequest, messaging::MessageSequence chunksObservable) {
  auto signatory = signedRequest->validate(*this->getRootCAs()); //The request itself is empty, but we do want to check the signature
  UserGroup::EnsureAccess(UserGroup::Authserver, signatory.organizationalUnit());
  assert(getStoragePath().has_value());
  backend_->ensureNoUserData();
  auto tmpUserDbMigrationPath = getStoragePath().value() / pep::filesystem::RandomizedName("AuthserverStorage-%%%%%%.sqlite");
  PEP_LOG(LogTag, Severity::Info) << "Received MigrateUserDbToAccessManagerRequest. Storing authserver storage as \"" << tmpUserDbMigrationPath.string() << '"';

  return chunksObservable.reduce(
    MakeStreamWithDeferredCleanup(tmpUserDbMigrationPath),
    [](auto streamWithCleanup, std::shared_ptr<std::string> chunk) {
      *streamWithCleanup.first << *chunk;
      return streamWithCleanup;
    },
    [tmpUserDbMigrationPath, backend=backend_](auto streamWithCleanup) -> messaging::MessageSequence {
      streamWithCleanup.first->close();
      return rxcpp::rxs::just(std::make_shared<std::string>(Serialization::ToString(backend->migrateUserDb(tmpUserDbMigrationPath))));
    }
  );
}

messaging::MessageBatches AccessManager::handleFindUserRequest(
    std::shared_ptr<SignedFindUserRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  return messaging::BatchSingleMessage(backend_->handleFindUserRequest(request, userGroup));
}

messaging::MessageBatches AccessManager::handleStructureMetadataRequest(std::shared_ptr<SignedStructureMetadataRequest> signedRequest) {
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  auto entries = backend_->handleStructureMetadataRequest(request, userGroup);
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
  auto certified = signedRequest->open(*this->getRootCAs());
  const auto& request = certified.message;
  auto userGroup = certified.signatory.organizationalUnit();

  backend_->handleSetStructureMetadataRequestHead(request, userGroup);

  return
      chunks.map([backend = backend_, subjectType = request.subjectType, userGroup
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
