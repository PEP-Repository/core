#include <pep/transcryptor/Transcryptor.hpp>

#include <pep/auth/EnrolledParty.hpp>
#include <pep/morphing/RepoKeys.hpp>
#include <pep/morphing/RepoRecipient.hpp>
#include <pep/rsk/RskSerializers.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>
#include <pep/transcryptor/TranscryptorSerializers.hpp>
#include <pep/utils/ApplicationMetrics.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/utils/Shared.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <prometheus/gauge.h>
#include <prometheus/summary.h>
#include <prometheus/text_serializer.h>

#include <chrono>
#include <numeric>

namespace pep {

const std::string LOG_TAG ("Transcryptor");
const severity_level TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY = debug;
const severity_level LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY = debug;
const severity_level CHECKSUM_CHAIN_CALCULATION_LOGGING_SEVERITY = debug;

Transcryptor::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry) :
  RegisteredMetrics(registry),
  keyComponent_request_duration(prometheus::BuildSummary()
    .Name("pep_transcryptor_keyComponent_request_duration_seconds")
    .Help("Duration of generating key component")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001} }, std::chrono::minutes{ 5 })),
  transcryptor_request_duration(prometheus::BuildSummary()
  .Name("pep_transcryptor_request_duration_seconds")
    .Help("Duration of a transcryptor request")
    .Register(*registry)
    .Add({}, prometheus::Summary::Quantiles{
      {0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001} }, std::chrono::minutes{ 5 })),
  transcryptor_log_size(prometheus::BuildGauge()
  .Name("pep_transcryptor_log_size_bytes")
    .Help("Size of transcryptor database in bytes")
    .Register(*registry)
    .Add({}))
  { }

Transcryptor::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : SigningServer::Parameters(io_context, config) {
  std::filesystem::path keysFile;
  std::filesystem::path systemKeysFile;
  std::filesystem::path storageFile;
  std::filesystem::path verifiersFile; // used to check RSK proofs made by access manager

  try {
    keysFile = config.get<std::filesystem::path>("KeysFile");

    if (auto optionalSystemKeysFile = config.get<std::optional<std::filesystem::path>>("SystemKeysFile")) {
      systemKeysFile = optionalSystemKeysFile.value();
    }
    else {
      //Legacy version, from when we still had a (Soft)HSM. TODO: use new version in configuration for all environments, and remove legacy version.
      systemKeysFile = config.get<std::filesystem::path>("HSM.ConfigFile");
    }

    storageFile = config.get<std::filesystem::path>("StorageFile");
    verifiersFile = config.get<std::filesystem::path>("VerifiersFile");
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with configuration file: " << e.what();
    throw;
  }

  try {
    Configuration keysConfig = Configuration::FromFile(keysFile);
    setPseudonymKey(ElgamalPrivateKey(boost::algorithm::unhex(
      keysConfig.get<std::string>("PseudonymKey"))));
  } catch (std::exception& e) {
    LOG(LOG_TAG, warning)
      << "Couldn't read pseudonymKey: " << e.what() << '\n'
      << "This is normal during first start-up when the Transcryptor still "
         "has to be enrolled with help from itself.  In other cases, "
         "this is an error" << std::endl;
  }

  boost::property_tree::ptree systemKeys;
  boost::property_tree::read_json(std::filesystem::absolute(systemKeysFile).string(), systemKeys);
  systemKeys = systemKeys.get_child_optional("Keys") //Old HSMKeys.json files have the keys in a Keys-object
      .get_value_or(systemKeys); //we now also allow them to be directly in the root, resulting in cleaner SystemKeys-files
  setPseudonymTranslator(std::make_shared<PseudonymTranslator>(ParsePseudonymTranslationKeys(systemKeys)));
  setDataTranslator(std::make_shared<DataTranslator>(ParseDataTranslationKeys(systemKeys)));

  setStorage(std::make_shared<TranscryptorStorage>(storageFile));

  setVerifiers(
    Serialization::FromJsonString<VerifiersResponse>(ReadFile(verifiersFile)));

}

std::shared_ptr<PseudonymTranslator> Transcryptor::Parameters::getPseudonymTranslator() const {
  return pseudonymTranslator;
}

std::shared_ptr<DataTranslator> Transcryptor::Parameters::getDataTranslator() const {
  return dataTranslator;
}

void Transcryptor::Parameters::setPseudonymTranslator(std::shared_ptr<PseudonymTranslator> pseudonymTranslator) {
  Parameters::pseudonymTranslator = pseudonymTranslator;
}

void Transcryptor::Parameters::setDataTranslator(std::shared_ptr<DataTranslator> dataTranslator) {
  Parameters::dataTranslator = dataTranslator;
}

void Transcryptor::Parameters::setStorage(std::shared_ptr<TranscryptorStorage> storage) {
  Parameters::storage = storage;
}
std::shared_ptr<TranscryptorStorage> Transcryptor::Parameters::getStorage() const {
  return storage;
}

void Transcryptor::Parameters::setPseudonymKey(const ElgamalPrivateKey& key) {
  Parameters::pseudonymKey = key;
}
std::optional<ElgamalPrivateKey> Transcryptor::Parameters::getPseudonymKey() const {
  return pseudonymKey;
}

void Transcryptor::Parameters::setVerifiers(const VerifiersResponse& verifiers) {
  Parameters::verifiers = verifiers;
}
const VerifiersResponse& Transcryptor::Parameters::getVerifiers() const {
  return verifiers.value();
}

void Transcryptor::Parameters::check() const {
  if(!pseudonymTranslator)
    throw std::runtime_error("pseudonymTranslator must be set");
  if(!dataTranslator)
    throw std::runtime_error("dataTranslator must be set");
  if(!storage)
    throw std::runtime_error("storage must be set");
  if(!verifiers)
    throw std::runtime_error("verifiers must be set");
  SigningServer::Parameters::check();
}

messaging::MessageBatches Transcryptor::handleKeyComponentRequest(std::shared_ptr<SignedKeyComponentRequest> signedRequest) {
  // Generate response
  auto start_time = std::chrono::steady_clock::now();
  auto response = KeyComponentResponse::HandleRequest(
    *signedRequest,
    *mPseudonymTranslator,
    *mDataTranslator,
    this->getRootCAs());

  lpMetrics->keyComponent_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count()); // in seconds

  //Return result
  return messaging::BatchSingleMessage(response);
}

messaging::MessageBatches Transcryptor::handleTranscryptorRequest(std::shared_ptr<TranscryptorRequest> request, messaging::MessageSequence entriesObservable) {
  auto start_time = std::chrono::steady_clock::now();
  auto requestNumber = mNextTranscryptorRequestNumber++;

  LOG(LOG_TAG, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << requestNumber << " received";

  if (!mPseudonymKey)
    throw Error("Transcryptor has not been enrolled with a PseudonymKey.");

  auto unpackedRequest = request->mRequest.openAsTranscryptor(this->getRootCAs());

  struct Context {
    uintmax_t requestNumber{};
    std::vector<std::string> modes;
    bool includeUserGroupPseudonyms{};
    SignedTicketRequest2 ticketRequest;
  };
  auto ctx = std::make_shared<Context>();
  ctx->requestNumber = requestNumber;
  ctx->modes = std::move(unpackedRequest.mModes);
  ctx->includeUserGroupPseudonyms = unpackedRequest.mIncludeUserGroupPseudonyms;
  ctx->ticketRequest = std::move(request->mRequest);

  struct Results {
    std::vector<LocalPseudonym> localPseudonyms;
    std::vector<LocalPseudonyms> responseEntries;
  };
  struct Batch {
    std::vector<TranscryptorRequestEntry> requestEntries;
    Results results;
  };

  auto server = SharedFrom(*this);
  LOG(LOG_TAG, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << requestNumber << " constructing observable";

  messaging::MessageBatches result = entriesObservable
    .map([](std::shared_ptr<std::string> serializedEntries) {
    auto deserialized = Serialization::FromString<TranscryptorRequestEntries>(*serializedEntries);
    auto batch = std::make_shared<Batch>();
    batch->requestEntries = std::move(deserialized.mEntries);
    batch->results.responseEntries.resize(batch->requestEntries.size());
    batch->results.localPseudonyms.resize(batch->requestEntries.size());
    return batch;
      })
    .concat_map([server, ctx](std::shared_ptr<Batch> batch) {
    std::vector<size_t> is(batch->requestEntries.size());
    LOG(LOG_TAG, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << ctx->requestNumber << " processing " << batch->requestEntries.size() << "-entry batch";
    std::iota(is.begin(), is.end(), 0);
    return server->mWorkerPool->batched_map<8>(std::move(is),
      observe_on_asio(*server->getIoContext()),
      [server, ctx, batch](size_t i) {
      const auto& entry = batch->requestEntries[i];
      auto& ret = batch->results.responseEntries[i];
      auto& localPseudonym = batch->results.localPseudonyms[i];

      if (ctx->includeUserGroupPseudonyms) {
        if (!entry.mUserGroup)
          throw Error("AccessGroup pseudonym missing "
            "even though includeAccessGroupPseudonyms is set");
        if (!entry.mUserGroupProof)
          throw Error("AccessGroup RSKProof missing "
            "even though includeAccessGroupPseudonyms is set");
      }
      else {
        if (entry.mUserGroup)
          throw Error("AccessGroup pseudonym set even though "
            "includeAccessGroupPseudonyms is not set");
        if (entry.mUserGroupProof)
          throw Error("AccessGroup RSKProof set even though "
            "includeAccessGroupPseudonyms is not set");
      }

      // Verify that the AM has properly RSKed the pseudonyms.
      try {
        server->mPseudonymTranslator->checkTranslationProof(
            entry.mPolymorphic, entry.mAccessManager,
            entry.mAccessManagerProof, server->mVerifiers.mAccessManager);
        server->mPseudonymTranslator->checkTranslationProof(
            entry.mPolymorphic, entry.mStorageFacility,
            entry.mStorageFacilityProof, server->mVerifiers.mStorageFacility);
        server->mPseudonymTranslator->checkTranslationProof(
            entry.mPolymorphic, entry.mTranscryptor,
            entry.mTranscryptorProof, server->mVerifiers.mTranscryptor);

        // TODO verify access group pseudonym
      }
      catch (const InvalidProof&) {
        throw Error("RSK Proof invalid");
      }

      // All seems fine: create final encrypted pseudonyms
      ret.mPolymorphic = entry.mPolymorphic;
      ret.mStorageFacility = server->mPseudonymTranslator->translateStep(
          entry.mStorageFacility,
          RecipientForServer(EnrolledParty::StorageFacility));
      ret.mAccessManager = server->mPseudonymTranslator->translateStep(
          entry.mAccessManager,
          RecipientForServer(EnrolledParty::AccessManager));
      localPseudonym = server->mPseudonymTranslator->translateStep(
          entry.mTranscryptor,
          RecipientForServer(EnrolledParty::Transcryptor)
      ).decrypt(*server->mPseudonymKey);

      if (ctx->includeUserGroupPseudonyms) {
        ret.mAccessGroup = server->mPseudonymTranslator->translateStep(
            *entry.mUserGroup,
            RecipientForCertificate(ctx->ticketRequest.mLogSignature->getLeafCertificate())
        );
      }

      localPseudonym.ensurePacked();
      ret.ensurePacked(); // prepack pseudonyms
      return i;
      })
      .map([batch](const std::vector<size_t>& unused [[maybe_unused]] ) {return batch; });
      })
    .reduce(
      std::make_shared<Results>(),
      [](std::shared_ptr<Results> results, std::shared_ptr<Batch> batch) {
        results->responseEntries.reserve(results->responseEntries.size() + batch->results.responseEntries.size());
        results->localPseudonyms.reserve(results->localPseudonyms.size() + batch->results.localPseudonyms.size());
        std::copy(batch->results.responseEntries.cbegin(), batch->results.responseEntries.cend(), std::back_insert_iterator(results->responseEntries));
        std::copy(batch->results.localPseudonyms.cbegin(), batch->results.localPseudonyms.cend(), std::back_insert_iterator(results->localPseudonyms));
        return results;
      })
    .map([server, ctx, start_time](std::shared_ptr<Results> results) {
      LOG(LOG_TAG, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << ctx->requestNumber << " finishing up";
      // Compute hash of encrypted local pseudonyms to check later that the AM
      // didn't change them.
      auto pseudonymHash = ComputePseudonymHash(results->responseEntries);

      TranscryptorResponse response;
      response.mEntries = std::move(results->responseEntries);
      response.mId = server->mStorage->logTicketRequest(
        results->localPseudonyms,
        ctx->modes,
        std::move(ctx->ticketRequest),
        pseudonymHash
      );
      auto result = rxcpp::observable<>::just(std::make_shared<std::string>(Serialization::ToString(std::move(response)))).as_dynamic();
      server->lpMetrics->transcryptor_request_duration.Observe(std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count()); // in seconds
      LOG(LOG_TAG, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << ctx->requestNumber << " returning result to requestor";
      return result;
    });

  result = rxcpp::observable<>::empty<messaging::MessageSequence>()
    .tap(
      [](auto) { /*ignore */},
      [](std::exception_ptr) { /*ignore */},
      [ctx]() {
        LOG(LOG_TAG, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << ctx->requestNumber << " starting asynchronous processing";
      })
    .concat(result);

  LOG(LOG_TAG, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << requestNumber << " returning observable";
  return result;
}

std::string Transcryptor::ComputePseudonymHash(
    const std::vector<LocalPseudonyms>& lps) {
  Sha512 hash;
  for (const auto& lp : lps) {
    hash.update(lp.mAccessManager.text());
    hash.update(lp.mStorageFacility.text());
    hash.update(lp.mPolymorphic.text());

    if (lp.mAccessGroup) {
      hash.update("y");
      hash.update(lp.mAccessGroup->text());
    } else {
      hash.update("n");
    }
  }
  return std::move(hash).digest();
}

messaging::MessageBatches
Transcryptor::handleLogIssuedTicketRequest(
    std::shared_ptr<LogIssuedTicketRequest> request) {
  auto requestNumber = mNextLogIssuedTicketRequestNumber++;
  LOG(LOG_TAG, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " received";

  auto ticket = request->mTicket.openForLogging(getRootCAs());
  LOG(LOG_TAG, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " opened ticket";

  auto hash = ComputePseudonymHash(ticket.mPseudonyms);
  LOG(LOG_TAG, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " calculated hash";

  mStorage->logIssuedTicket(
    request->mId,
    hash,
    std::move(ticket.mColumns),
    std::move(ticket.mModes),
    ticket.mUserGroup,
    ticket.mTimestamp
  );

  LOG(LOG_TAG, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " finishing up";
  auto result = messaging::BatchSingleMessage(
      LogIssuedTicketResponse(
        Signature::Make(
          request->mTicket.mData,
          *this->getSigningIdentity()
        )
      )
    );

  LOG(LOG_TAG, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " returning result to requestor";
  return result;
}

messaging::MessageBatches Transcryptor::handleRekeyRequest(std::shared_ptr<RekeyRequest> pRequest) {
  if (!pRequest->mClientCertificateChain.verify(getRootCAs())) {
    throw Error("Client certificate chain is not valid");
  }
  const auto party = GetEnrolledParty(pRequest->mClientCertificateChain);
  if (!party.has_value()) {
    throw Error("Cannot rekey for this requestor");
  }
  if (!HasDataAccess(*party)) {
    throw std::runtime_error("Requestor does not have data access: " + std::to_string(static_cast<unsigned>(*party)));
  }

  const auto recipient = RekeyRecipientForCertificate(pRequest->mClientCertificateChain.front());

  return mWorkerPool->batched_map<8>(std::move(pRequest->mKeys),
          observe_on_asio(*getIoContext()),
      [server = SharedFrom(*this), recipient](EncryptedKey entry) {

    EncryptedKey retEntry = server->mDataTranslator->translateStep(entry, recipient);
    retEntry.ensurePacked();

    return retEntry;
  }).map([](std::vector<EncryptedKey> keys){
    RekeyResponse resp;
    resp.mKeys = std::move(keys);
    return rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(std::move(resp)))).as_dynamic();
  });
}

Transcryptor::Transcryptor(std::shared_ptr<Parameters> parameters)
  : SigningServer(parameters),
  mWorkerPool(WorkerPool::getShared()),
  mPseudonymKey(parameters->getPseudonymKey()),
  mPseudonymTranslator(parameters->getPseudonymTranslator()),
  mDataTranslator(parameters->getDataTranslator()),
  mStorage(parameters->getStorage()),
  lpMetrics(std::make_shared<Metrics>(mRegistry)),
  mVerifiers(parameters->getVerifiers()) {
  RegisterRequestHandlers(*this,
                          &Transcryptor::handleKeyComponentRequest,
                          &Transcryptor::handleTranscryptorRequest,
                          &Transcryptor::handleRekeyRequest,
                          &Transcryptor::handleLogIssuedTicketRequest);

  mVerifiers.ensureThreadSafe(); // See #791
}

std::string Transcryptor::describe() const {
  return "Transcryptor";
}

std::optional<std::filesystem::path> Transcryptor::getStoragePath() {
  std::filesystem::path path = (std::filesystem::path(mStorage->getPath())).remove_filename();
  return EnsureDirectoryPath(path);
}

std::shared_ptr<prometheus::Registry>
Transcryptor::getMetricsRegistry()  {
  // Collect some metrics ad hoc
  lpMetrics->transcryptor_log_size.Set(static_cast<double>(
        std::filesystem::file_size(mStorage->getPath())));

  // Collect the base metrics and return the complete registry
  return SigningServer::getMetricsRegistry();
}

std::vector<std::string> Transcryptor::getChecksumChainNames() const {
  return mStorage->getChecksumChainNames();
}

void Transcryptor::computeChecksumChainChecksum(
    const std::string& chain, std::optional<uint64_t> maxCheckpoint, uint64_t& checksum,
    uint64_t& checkpoint) {
  std::string when;
  if (maxCheckpoint.has_value()) {
    when = " at checkpoint " + std::to_string(*maxCheckpoint);
  }
  LOG(LOG_TAG, CHECKSUM_CHAIN_CALCULATION_LOGGING_SEVERITY) << "Starting calculation for checksum chain " << chain << when;
  mStorage->computeChecksum(chain, maxCheckpoint, checksum, checkpoint);
  LOG(LOG_TAG, CHECKSUM_CHAIN_CALCULATION_LOGGING_SEVERITY) << "Finished calculation for checksum chain " << chain << when;
}

}
