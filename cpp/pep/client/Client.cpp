#include <pep/client/Client.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/registrationserver/RegistrationServerSerializers.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/authserver/AuthserverSerializers.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>

namespace pep {

namespace {

const std::string LOG_TAG("Client");

}

rxcpp::observable<std::string> Client::registerParticipant(const ParticipantPersonalia& personalia,
                                                           bool isTestParticipant,
                                                           const std::string& studyContext,
                                                           bool complete) {
  if ((personalia.getFirstName().empty() && personalia.getMiddleName().empty() && personalia.getLastName().empty())
      || personalia.getDateOfBirth().empty()) {
    throw std::runtime_error("Personal data are needed to register a participant");
  }

  struct CellProperties {
    std::shared_ptr<std::string> value;
    std::string fileExtension;

    CellProperties(const std::string& value, const std::string& fileExtension)
      : value(MakeSharedCopy(value)),
        fileExtension(fileExtension) {
    }
  };

  auto values = std::make_shared<std::map<std::string, CellProperties>>();
  values->emplace("StudyContexts", CellProperties{studyContext, ".csv"});
  values->emplace("ParticipantInfo", CellProperties{personalia.toJson(), ".json"});
  values->emplace("IsTestParticipant", CellProperties{BoolToString(isTestParticipant), ".txt"});

  return this
      ->getInaccessibleColumns("write",
                               rxcpp::observable<>::iterate(*values).map([](const auto& pair) { return pair.first; }))
      .op(RxToSet())
      .tap([](std::shared_ptr<std::set<std::string>> inaccessible) {
        if (!inaccessible->empty()) {
          throw std::runtime_error("Missing write access to " + std::to_string(inaccessible->size())
                                   + " required column(s), a.o. " + *inaccessible->cbegin());
        }
      })
      .as_dynamic() // Reduce compiler memory usage
      .concat_map([this](std::shared_ptr<std::set<std::string>> inaccessible) { return generatePEPID(); })
      .as_dynamic() // Reduce compiler memory usage
      .flat_map([this, values, complete](std::string identifier) {
        // Generate polymorphic pseudonym
        auto polymorphicPseudonym = MakeSharedCopy(generateParticipantPolymorphicPseudonym(identifier));

        // Create StoreData2Entry instances for the data-to-store
        std::vector<StoreData2Entry> entries;
        entries.reserve(values->size());
        std::transform(values->cbegin(),
                       values->cend(),
                       std::back_inserter(entries),
                       [polymorphicPseudonym](const auto& pair) {
                         const std::string& column = pair.first;
                         const CellProperties& props = pair.second;
                         StoreData2Entry result(polymorphicPseudonym, column, props.value);
                         if (!props.fileExtension.empty()) {
                           result.mXMetadata.emplace(MetadataXEntry::MakeFileExtension(props.fileExtension));
                         }
                         return result;
                       });

        // Store data in PEP
        return storeData2(entries)
            .last()       // Ensure further actions are executed only once
            .as_dynamic() // Reduce compiler memory usage
            .flat_map([this, identifier, complete](DataStorageResult2 result) {
              if (complete) {
                return completeParticipantRegistration(identifier, true);
              }
              else {
                auto response = std::make_shared<RegistrationResponse>();
                rxcpp::observable<std::shared_ptr<RegistrationResponse>> result = rxcpp::observable<>::from(response);
                return result;
              }
            })
            .as_dynamic() // Reduce compiler memory usage
            .last()       // Ensure further actions are executed only once
            .as_dynamic() // Reduce compiler memory usage
            .map([identifier](std::shared_ptr<RegistrationResponse> lpResponse) { return identifier; });
      });
}

// Generates a Participant ID and returns it
rxcpp::observable<std::string> Client::generatePEPID() {
  return clientRegistrationServer->requestIdRegistration()
      .map([](PEPIdRegistrationResponse result) { return result.mPepId; });
}

rxcpp::observable<std::shared_ptr<RegistrationResponse>> Client::completeParticipantRegistration(
    const std::string& identifier, bool skipIdentifierStorage) {
  auto pp = generateParticipantPolymorphicPseudonym(identifier);

  if (skipIdentifierStorage)
    return generateShortPseudonyms(pp, identifier);

  // Legacy: early participants were registered using an external identifier that was (initially) not stored. Completion
  // of such registrations must store the identifier retroactively
  return enumerateData2({},                        // groups
                        {pp},                      // pps
                        {},                        // columnGroups
                        {"ParticipantIdentifier"}) // columns
      .flat_map([this, identifier, pp](std::vector<EnumerateResult> result) -> rxcpp::observable<DataStorageResult2> {
        if (!result.empty()) {
          LOG(LOG_TAG, info) << "Participant identifier already present in PEP";
          // We do not store the participant ID again, but continue with the storage of other stuff in case this failed
          // before
          return rxcpp::observable<>::from(DataStorageResult2());
        }

        // Store participant identifier in PEP
        return storeData2(pp,
                          "ParticipantIdentifier",
                          std::make_shared<std::string>(identifier),
                          {MetadataXEntry::MakeFileExtension(".txt")});
      })
      .flat_map([this, pp, identifier](DataStorageResult2 result) { return generateShortPseudonyms(pp, identifier); });
}

rxcpp::observable<std::string> Client::listCastorImportColumns(const std::string& spColumnName,
                                                               const std::optional<unsigned>& answerSetCount) {
  return clientRegistrationServer
      ->requestListCastorImportColumns(ListCastorImportColumnsRequest{spColumnName, answerSetCount.value_or(0U)})
      .flat_map([](const ListCastorImportColumnsResponse& response) {
        return rxcpp::observable<>::iterate(response.mImportColumns);
      });
}

rxcpp::observable<std::shared_ptr<RegistrationResponse>> Client::generateShortPseudonyms(
    const PolymorphicPseudonym& pp, const std::string& identifier) {
  LOG(LOG_TAG, debug) << "Start generating short pseudonyms";

  RegistrationRequest request(pp);
  std::string encryptedIdentifier = publicKeyShadowAdministration.encrypt(identifier);
  request.mEncryptedIdentifier = encryptedIdentifier;
  request.mEncryptionPublicKeyPem = publicKeyShadowAdministration.toPem();

  LOG(LOG_TAG, debug) << "Sending RegistrationRequest...";

  return clientRegistrationServer->requestRegistration(std::move(request))
      .map([](RegistrationResponse result) { return std::make_shared<RegistrationResponse>(result); });
}

rxcpp::observable<EnrollmentResult> Client::enrollUser(const std::string& oauthToken) {

  LOG(LOG_TAG, debug) << "Generating key pair";
  AsymmetricKeyPair keyPair = AsymmetricKeyPair::GenerateKeyPair();
  LOG(LOG_TAG, debug) << "Key pair generated";
  LOG(LOG_TAG, debug) << "Generating CSR";

  // Parse OAuth token
  auto token = OAuthToken::Parse(oauthToken);

  // Generate CSR
  X509CertificateSigningRequest csr(keyPair, token.getSubject(), token.getGroup());

  LOG(LOG_TAG, debug) << "Generated CSR for CN=" << csr.getCommonName().value_or("") << " and OU=" << csr.getOrganizationalUnit().value_or("");

  auto privateKey = std::make_shared<AsymmetricKey>(keyPair.getPrivateKey());

  EnrollmentRequest request(csr, oauthToken);
  LOG(LOG_TAG, debug) << "Sending EnrollmentRequest...";
  return clientKeyServer->requestUserEnrollment(std::move(request))
    .flat_map([this, privateKey](EnrollmentResponse lpResponse) {
    auto ctx = std::make_shared<EnrollmentContext>();
    ctx->identity = std::make_shared<X509Identity>(*privateKey, lpResponse.mCertificateChain);

    return completeEnrollment(ctx);
      });
}

rxcpp::observable<std::string> Client::requestToken(std::string subject,
                                                       std::string group,
                                                       Timestamp expirationTime) {
  return this->getAuthClient()->requestToken(TokenRequest(subject, group, expirationTime)) // linebreak
      .map([](TokenResponse response) { return response.mToken; });
}

std::shared_ptr<const KeyClient> Client::getKeyClient(bool require) const {
  return GetConstServerProxy(clientKeyServer, "Key Server", require);
}

std::shared_ptr<const AuthClient> Client::getAuthClient(bool require) const {
  return GetConstServerProxy(clientAuthserver, "Auth Server", require);
}

std::shared_ptr<const RegistrationClient> Client::getRegistrationClient(bool require) const {
  return GetConstServerProxy(clientRegistrationServer, "Registration Server", require);
}

rxcpp::observable<FakeVoid> Client::shutdown() {
  return CoreClient::shutdown()
    .merge(clientKeyServer->shutdown())
    .merge(clientRegistrationServer->shutdown())
    .merge(clientAuthserver ? clientAuthserver->shutdown() : rxcpp::rxs::empty<FakeVoid>().as_dynamic())
    .last();
}

Client::Client(const Builder& builder)
  : CoreClient(builder),
    publicKeyShadowAdministration(builder.getPublicKeyShadowAdministration()),
    keyServerEndPoint(builder.getKeyServerEndPoint()),
    authserverEndPoint(builder.getAuthserverEndPoint()),
    registrationServerEndPoint(builder.getRegistrationServerEndPoint()) {
  clientKeyServer = this->tryConnectServerProxy<KeyClient>(keyServerEndPoint);
  clientAuthserver = this->tryConnectServerProxy<AuthClient>(authserverEndPoint);
  clientRegistrationServer = this->tryConnectServerProxy<RegistrationClient>(registrationServerEndPoint);
}


void Client::Builder::initialize(const Configuration& config,
                                     std::shared_ptr<boost::asio::io_context> io_context,
                                     bool persistKeysFile) {
  CoreClient::Builder::initialize(config, io_context, persistKeysFile);

  if (auto shadowPublicKeyFile = config.get<std::optional<std::filesystem::path>>("ShadowPublicKeyFile")) {
    this->setPublicKeyShadowAdministration(AsymmetricKey(ReadFile(*shadowPublicKeyFile)));
  }

  if (auto ksConfig = config.get<std::optional<EndPoint>>("KeyServer")) {
    this->setKeyServerEndPoint(*ksConfig);
  }

  if (auto asConfig = config.get<std::optional<EndPoint>>("Authserver")) {
    this->setAuthserverEndPoint(*asConfig);
  }

  if (auto rsConfig = config.get<std::optional<EndPoint>>("RegistrationServer")) {
    this->setRegistrationServerEndPoint(*rsConfig);
  }
}

std::shared_ptr<Client> Client::OpenClient(const Configuration& config,
                                                   std::shared_ptr<boost::asio::io_context> io_context,
                                                   bool persistKeysFile) {
  Builder builder;
  builder.initialize(config, io_context, persistKeysFile);
  return builder.build();
}

} // namespace pep
