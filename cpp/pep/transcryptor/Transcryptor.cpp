#include <pep/transcryptor/Transcryptor.hpp>

#include <pep/auth/EnrolledParty.hpp>
#include <pep/morphing/MorphingPropertySerializers.hpp>
#include <pep/morphing/RepoKeys.hpp>
#include <pep/morphing/RepoRecipient.hpp>
#include <pep/morphing/MorphingSerializers.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/rsk/RskSerializers.hpp>
#include <pep/transcryptor/TranscryptorSerializers.hpp>
#include <pep/utils/ApplicationMetrics.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/OpenSSLHasher.hpp>
#include <pep/utils/Shared.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <prometheus/gauge.h>
#include <prometheus/summary.h>
#include <prometheus/text_serializer.h>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-tap.hpp>

#include <chrono>
#include <numeric>

namespace pep {

const std::string LogTag ("Transcryptor");
const Severity TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY = Severity::Debug;
const Severity LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY = Severity::Debug;
const Severity CHECKSUM_CHAIN_CALCULATION_LOGGING_SEVERITY = Severity::Debug;

Transcryptor::Metrics::Metrics(std::shared_ptr<prometheus::Registry> registry) :
  RegisteredMetrics(registry),
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
  : KeyComponentServer::Parameters(io_context, config) {
  std::filesystem::path keysFile;
  std::filesystem::path storageFile;
  std::filesystem::path verifiersFile; // used to check RSK proofs made by access manager

  try {
    keysFile = config.get<std::filesystem::path>("EnrolledPartyKeysFile");
    storageFile = config.get<std::filesystem::path>("StorageFile");
    verifiersFile = config.get<std::filesystem::path>("VerifiersFile");

    auto serverEndPoints = config.get_child("ServerEndPoints");
    accessManagerEndPoint = serverEndPoints.get<EndPoint>(ServerTraits::AccessManager().configNode());
  }
  catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Critical) << "Error with configuration file: " << e.what();
    throw;
  }

  try {
    auto enrolledPartyKeys = Configuration::FromFile(keysFile).get<EnrolledPartyKeys>("");
    setPseudonymKey(enrolledPartyKeys.pseudonymKey.value());
  } catch (std::exception& e) {
    PEP_LOG(LogTag, Severity::Warning)
      << "Couldn't read pseudonymKey: " << e.what() << '\n'
      << "This is normal during first start-up when the Transcryptor still "
         "has to be enrolled with help from itself.  In other cases, "
         "this is an error" << std::endl;
  }

  setStorage(std::make_shared<TranscryptorStorage>(storageFile));

  try {
    setVerifiers(
      Serialization::FromJsonString<ServerVerifiers>(ReadFile(verifiersFile)));
  } catch (...) {
    PEP_LOG(LogTag, Severity::Error) << "Failed to load verifiers from " << verifiersFile;
    throw;
  }
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

void Transcryptor::Parameters::setVerifiers(const ServerVerifiers& verifiers) {
  Parameters::verifiers = verifiers;
}
const ServerVerifiers& Transcryptor::Parameters::getVerifiers() const {
  return verifiers.value();
}

void Transcryptor::Parameters::check() const {
  if(!storage)
    throw std::runtime_error("storage must be set");
  if(!verifiers)
    throw std::runtime_error("verifiers must be set");
  KeyComponentServer::Parameters::check();
}

messaging::MessageBatches Transcryptor::handleTranscryptorRequest(std::shared_ptr<TranscryptorRequest> request, messaging::MessageSequence entriesObservable) {
  auto start_time = std::chrono::steady_clock::now();
  auto requestNumber = mNextTranscryptorRequestNumber++;

  PEP_LOG(LogTag, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << requestNumber << " received";

  if (!mPseudonymKey)
    throw Error("Transcryptor has not been enrolled with a PseudonymKey.");

  auto certified = request->mRequest.openAsTranscryptor(*this->getRootCAs());
  const auto& userRequest = certified.message;
  auto userCertificate = certified.signatory.certificateChain().leaf();
  auto domain = userCertificate.getOrganizationalUnit().value();

  auto server = SharedFrom(*this);

  auto userVerifiersObs = rxcpp::observable<>::just<std::optional<ReshuffleRekeyVerifiers>>(std::nullopt)
    .as_dynamic();
  if (userRequest.mIncludeUserGroupPseudonyms) {
    if (auto userVerifiers = mStorage->getUserVerifiers(userCertificate)) {
      // Use stored verifiers
      userVerifiersObs = rxcpp::observable<>::just(userVerifiers);
    } else {
      PEP_LOG(LogTag, Severity::Debug) << "Requesting verifiers from AccessManager for "
        << Logging::Escape(userCertificate.getCommonName().value()) << " in " << Logging::Escape(domain);
      userVerifiersObs = mAccessManagerProxy.requestUserVerifiers({userCertificate})
        .map([server, userCertificate](const UserVerifiersResponse& response) {
          // Check internal consistency of verifiers
          auto verifiers = response.open(server->systemPublicKeys().globalPseudonymEncryptionKey);
          // Cross-reference with existing verifiers and store if consistent
          server->mStorage->checkAndStoreUserVerifiers(userCertificate, verifiers);
          return std::optional{verifiers};
        });
    }
  }

  struct Context {
    uintmax_t requestNumber{};
    std::vector<std::string> modes;
    bool includeUserGroupPseudonyms{};
    SignedTicketRequest2 ticketRequest;
    std::optional<PseudonymTranslator::Recipient> userRecipient;
    std::optional<ReshuffleRekeyVerifiers> userVerifiers{};
  };
  auto ctx = MakeSharedCopy(Context{
    .requestNumber = requestNumber,
    .modes = userRequest.mModes,
    .includeUserGroupPseudonyms = userRequest.mIncludeUserGroupPseudonyms,
    .ticketRequest = std::move(request->mRequest),
    .userRecipient = userRequest.mIncludeUserGroupPseudonyms
      ? std::optional{RecipientForCertificate(userCertificate)}
      : std::nullopt,
    });

  struct Results {
    std::vector<LocalPseudonym> localPseudonyms;
    std::vector<LocalPseudonyms> responseEntries;
  };
  struct Batch {
    std::vector<TranscryptorRequestEntry> requestEntries;
    Results results;
  };

  PEP_LOG(LogTag, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << requestNumber << " constructing observable";

  messaging::MessageBatches result =
    userVerifiersObs
    .flat_map([server, ctx, entriesObservable, start_time](const std::optional<ReshuffleRekeyVerifiers>& userVerifiers) {
    ctx->userVerifiers = userVerifiers;
    return entriesObservable
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
    PEP_LOG(LogTag, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << ctx->requestNumber << " processing " << batch->requestEntries.size() << "-entry batch";
    std::iota(is.begin(), is.end(), 0);
    return server->mWorkerPool->batched_map<8>(std::move(is),
      observe_on_asio(*server->getIoContext()),
      [server, ctx, batch](size_t i) {
      const auto& entry = batch->requestEntries[i];
      auto& ret = batch->results.responseEntries[i];
      auto& localPseudonym = batch->results.localPseudonyms[i];

      if (ctx->includeUserGroupPseudonyms) {
        if (!entry.userGroup_)
          throw Error("AccessGroup pseudonym missing "
            "even though includeAccessGroupPseudonyms is set");
        if (!entry.mUserGroupProof)
          throw Error("AccessGroup RskProof missing "
            "even though includeAccessGroupPseudonyms is set");
      }
      else {
        if (entry.userGroup_)
          throw Error("AccessGroup pseudonym set even though "
            "includeAccessGroupPseudonyms is not set");
        if (entry.mUserGroupProof)
          throw Error("AccessGroup RskProof set even though "
            "includeAccessGroupPseudonyms is not set");
      }

      const PseudonymTranslator& pseudonymTranslator = server->pseudonymTranslator();
      // Verify that the AM has properly RSKed the pseudonyms.
      try {
        pseudonymTranslator.checkTranslationProof(
            entry.mPolymorphic, entry.mAccessManager,
            entry.mAccessManagerProof, server->mVerifiers.accessManager);
        pseudonymTranslator.checkTranslationProof(
            entry.mPolymorphic, entry.mStorageFacility,
            entry.mStorageFacilityProof, server->mVerifiers.storageFacility);
        pseudonymTranslator.checkTranslationProof(
            entry.mPolymorphic, entry.mTranscryptor,
            entry.mTranscryptorProof, server->mVerifiers.transcryptor);

        if (ctx->includeUserGroupPseudonyms) {
          pseudonymTranslator.checkTranslationProof(
              entry.mPolymorphic, *entry.userGroup_,
              *entry.mUserGroupProof, *ctx->userVerifiers);
        }
      }
      catch (const InvalidProof&) {
        throw Error("RSK Proof invalid");
      }

      // All seems fine: create final encrypted pseudonyms
      ret.mPolymorphic = entry.mPolymorphic;
      ret.mStorageFacility = pseudonymTranslator.translateStep(
          entry.mStorageFacility,
          RecipientForServer(EnrolledParty::StorageFacility));
      ret.mAccessManager = pseudonymTranslator.translateStep(
          entry.mAccessManager,
          RecipientForServer(EnrolledParty::AccessManager));
      localPseudonym = pseudonymTranslator.translateStep(
          entry.mTranscryptor,
          RecipientForServer(EnrolledParty::Transcryptor)
      ).decrypt(*server->mPseudonymKey);

      if (ctx->includeUserGroupPseudonyms) {
        ret.mAccessGroup = pseudonymTranslator.translateStep(
            *entry.userGroup_,
            *ctx->userRecipient);
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
      PEP_LOG(LogTag, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << ctx->requestNumber << " finishing up";
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
      PEP_LOG(LogTag, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << ctx->requestNumber << " returning result to requestor";
      return result;
    });
    });

  result = rxcpp::observable<>::empty<messaging::MessageSequence>()
    .tap(
      [](auto) { /*ignore */},
      [](std::exception_ptr) { /*ignore */},
      [ctx]() {
        PEP_LOG(LogTag, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << ctx->requestNumber << " starting asynchronous processing";
      })
    .concat(result);

  PEP_LOG(LogTag, TRANSCRYPTOR_REQUEST_LOGGING_SEVERITY) << "Transcryptor request " << requestNumber << " returning observable";
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
  PEP_LOG(LogTag, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " received";

  std::string serialized;
  auto ticket = request->ticket_.openForLogging(*getRootCAs(), serialized);
  PEP_LOG(LogTag, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " opened ticket";

  auto hash = ComputePseudonymHash(ticket.mAccessSubjects);
  PEP_LOG(LogTag, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " calculated hash";

  mStorage->logIssuedTicket(
    request->mId,
    hash,
    std::move(ticket.mColumns),
    std::move(ticket.mModes),
    ticket.userGroup_,
    ticket.mTimestamp
  );

  PEP_LOG(LogTag, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " finishing up";
  auto result = messaging::BatchSingleMessage(
      LogIssuedTicketResponse(
        Signature::Make(
          serialized,
          *this->getSigningIdentity()
        )
      )
    );

  PEP_LOG(LogTag, LOG_ISSUED_TICKET_REQUEST_LOGGING_SEVERITY) << "LogIssuedTicket request " << requestNumber << " returning result to requestor";
  return result;
}

messaging::MessageBatches Transcryptor::handleRekeyRequest(std::shared_ptr<RekeyRequest> pRequest) {
  if (!pRequest->mClientCertificateChain.verify(*getRootCAs())) {
    throw Error("Client certificate chain is not valid");
  }
  const auto party = GetEnrolledParty(pRequest->mClientCertificateChain);
  if (!party.has_value()) {
    throw Error("Cannot rekey for this requestor");
  }
  if (!HasDataAccess(*party)) {
    throw std::runtime_error("Requestor does not have data access: " + std::to_string(static_cast<unsigned>(*party)));
  }

  const auto recipient = RekeyRecipientForCertificate(pRequest->mClientCertificateChain.leaf());

  return mWorkerPool->batched_map<8>(std::move(pRequest->mKeys),
          observe_on_asio(*getIoContext()),
      [server = SharedFrom(*this), recipient](EncryptedKey entry) {

    EncryptedKey retEntry = server->dataTranslator().translateStep(entry, recipient);
    retEntry.ensurePacked();

    return retEntry;
  }).map([](std::vector<EncryptedKey> keys){
    RekeyResponse resp;
    resp.mKeys = std::move(keys);
    return rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(std::move(resp)))).as_dynamic();
  });
}

Transcryptor::Transcryptor(std::shared_ptr<Parameters> parameters)
  : KeyComponentServer(parameters),
  mWorkerPool(WorkerPool::getShared()),
  mPseudonymKey(parameters->getPseudonymKey()),
  mAccessManagerProxy(messaging::ServerConnection::Create(this->getIoContext(), parameters->getAccessManagerEndPoint(), parameters->getRootCACertificatesFilePath()), *this, parameters->getAccessManagerEndPoint().expectedCommonName, getRootCAs()),
  mStorage(parameters->getStorage()),
  lpMetrics(std::make_shared<Metrics>(mRegistry)),
  mVerifiers(parameters->getVerifiers()) {
  RegisterRequestHandlers(*this,
                          &Transcryptor::handleTranscryptorRequest,
                          &Transcryptor::handleRekeyRequest,
                          &Transcryptor::handleLogIssuedTicketRequest);

  mVerifiers.ensureThreadSafe(); // See #791
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
  PEP_LOG(LogTag, CHECKSUM_CHAIN_CALCULATION_LOGGING_SEVERITY) << "Starting calculation for checksum chain " << chain << when;
  mStorage->computeChecksum(chain, maxCheckpoint, checksum, checkpoint);
  PEP_LOG(LogTag, CHECKSUM_CHAIN_CALCULATION_LOGGING_SEVERITY) << "Finished calculation for checksum chain " << chain << when;
}

}
