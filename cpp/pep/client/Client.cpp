#include <pep/client/Client.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/async/RxToEmpty.hpp>
#include <pep/async/RxToSet.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/auth/OAuthToken.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-tap.hpp>
#include <rxcpp/operators/rx-zip.hpp>

namespace pep {

namespace {

const std::string LOG_TAG("Client");

}

rxcpp::observable<std::string> Client::getInaccessibleColumns(const std::string& mode, rxcpp::observable<std::string> columns) {
  return columns.op(RxToSet()) // Create a set of columns we still need to check
    .zip(this->getAccessManagerProxy()->getAccessibleColumns(true)) // Combine it with "the stuff we have access to"
    .flat_map([mode](const auto& context) {
    // Extract named variables from the context tuple
    std::shared_ptr<std::set<std::string>> remaining = std::get<0>(context);
    const ColumnAccess& access = std::get<1>(context);
    // For every column group...
    for (const auto& cg : access.columnGroups) {
      const auto& cgAccess = cg.second;
      if (std::find(cgAccess.modes.cbegin(), cgAccess.modes.cend(), mode) != cgAccess.modes.cend()) { // ...if we have the requested access mode to that group...
        for (auto index : cgAccess.columns.mIndices) { // ...remove the associated columns from the set-of-columns-that-we-need-to-check
          remaining->erase(access.columns[index]);
        }
      }
    }

    // Columns that haven't been checked are inaccessible: return them
    return RxIterate(*remaining);
      });
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
                               RxIterate(*values).map([](const auto& pair) { return pair.first; }))
      .op(RxToSet())
      .tap([](std::shared_ptr<std::set<std::string>> inaccessible) {
        if (!inaccessible->empty()) {
          throw std::runtime_error("Missing write access to " + std::to_string(inaccessible->size())
                                   + " required column(s), a.o. " + *inaccessible->cbegin());
        }
      })
      .as_dynamic() // Reduce compiler memory usage
      .concat_map([this](std::shared_ptr<std::set<std::string>> inaccessible) { return this->getRegistrationServerProxy()->registerPepId(); })
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
        auto process = storeData2(entries).op(RxToEmpty<FakeVoid>());
        if (complete) {
          process = process.flat_map([this, identifier](FakeVoid) {
            return completeParticipantRegistration(identifier, true);
            });
        }

        // Return the identifier instead of the FakeVoid instance(s) that we got from processing
        return process.op(RxInstead(identifier));
      });
}

rxcpp::observable<FakeVoid> Client::completeParticipantRegistration(
    const std::string& identifier, bool skipIdentifierStorage) {
  auto pp = generateParticipantPolymorphicPseudonym(identifier);

  if (skipIdentifierStorage)
    return generateShortPseudonyms(pp, identifier);

  // Legacy: early participants were registered using an external identifier that was (initially) not stored. Completion
  // of such registrations must store the identifier retroactively
  return enumerateData({},                        // groups
                        {pp},                      // pps
                        {},                        // columnGroups
                        {"ParticipantIdentifier"}) // columns
      .flat_map([this, identifier, pp](std::vector<std::shared_ptr<EnumerateResult>> result) -> rxcpp::observable<DataStorageResult2> {
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

rxcpp::observable<FakeVoid> Client::generateShortPseudonyms(PolymorphicPseudonym pp, const std::string& identifier) {
  LOG(LOG_TAG, debug) << "Sending RegistrationRequest...";
  return registrationServerProxy->completeShortPseudonyms(pp, identifier, publicKeyShadowAdministration);
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
  return keyServerProxy->requestUserEnrollment(std::move(request))
    .flat_map([this, privateKey](EnrollmentResponse lpResponse) {
    auto ctx = std::make_shared<EnrollmentContext>();
    ctx->identity = std::make_shared<X509Identity>(*privateKey, lpResponse.mCertificateChain);

    return completeEnrollment(ctx);
      });
}

std::shared_ptr<const KeyServerProxy> Client::getKeyServerProxy(bool require) const {
  return GetConstServerProxy(keyServerProxy, ServerTraits::KeyServer(), require);
}

std::shared_ptr<const AuthServerProxy> Client::getAuthServerProxy(bool require) const {
  return GetConstServerProxy(authServerProxy, ServerTraits::AuthServer(), require);
}

std::shared_ptr<const RegistrationServerProxy> Client::getRegistrationServerProxy(bool require) const {
  return GetConstServerProxy(registrationServerProxy, ServerTraits::RegistrationServer(), require);
}

Client::ServerProxies Client::getServerProxies(bool requireAll) const {
  auto result = CoreClient::getServerProxies(requireAll);
  AddServerProxy(result, ServerTraits::AuthServer(), this->getAuthServerProxy(requireAll));
  AddServerProxy(result, ServerTraits::KeyServer(), this->getKeyServerProxy(requireAll));
  AddServerProxy(result, ServerTraits::RegistrationServer(), this->getRegistrationServerProxy(requireAll));
  return result;
}

rxcpp::observable<FakeVoid> Client::shutdown() {
  return CoreClient::shutdown()
    .merge(keyServerProxy->shutdown())
    .merge(registrationServerProxy->shutdown())
    .merge(authServerProxy ? authServerProxy->shutdown() : rxcpp::rxs::empty<FakeVoid>().as_dynamic())
    .last();
}

Client::Client(const Builder& builder)
  : CoreClient(builder),
    publicKeyShadowAdministration(builder.getPublicKeyShadowAdministration()),
    keyServerEndPoint(builder.getKeyServerEndPoint()),
    authserverEndPoint(builder.getAuthserverEndPoint()),
    registrationServerEndPoint(builder.getRegistrationServerEndPoint()) {
  keyServerProxy = this->tryConnectServerProxy<KeyServerProxy>(keyServerEndPoint);
  authServerProxy = this->tryConnectServerProxy<AuthServerProxy>(authserverEndPoint);
  registrationServerProxy = this->tryConnectServerProxy<RegistrationServerProxy>(registrationServerEndPoint);
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
