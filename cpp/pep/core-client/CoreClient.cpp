#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/async/IoContextThread.hpp>
#include <pep/async/RxCache.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/elgamal/CurvePoint.PropertySerializer.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/rsk/RskSerializers.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/structure/ColumnNameSerializers.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>
#include <pep/utils/Compare.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/CollectionUtils.hpp>
#include <pep/utils/Platform.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-observe_on.hpp>
#include <rxcpp/operators/rx-reduce.hpp>
#include <rxcpp/operators/rx-merge.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-take.hpp>
#include <rxcpp/operators/rx-on_error_resume_next.hpp>
#include <rxcpp/operators/rx-switch_if_empty.hpp>
#include <rxcpp/operators/rx-zip.hpp>

namespace pep {

namespace {

const std::string LOG_TAG ("CoreClient");

bool ModesInclude(const std::vector<std::string>& required, std::vector<std::string> provided) {
  auto begin = provided.cbegin(), end = provided.cend();
  // If a "read" privilege is held, ensure that the corresponding (implicitly included) "read-meta" privilege is in the array as well
  if (std::find(begin, end, "read") != end && std::find(begin, end, "read-meta") == end) {
    provided.push_back("read-meta");

    // Ensure that iterators are valid after vector update
    begin = provided.cbegin();
    end = provided.cend();
  }
  // If a "write-meta" privilege is held, ensure that the corresponding (implicitly included) "write" privilege is in the array as well
  if (std::find(begin, end, "write-meta") != end && std::find(begin, end, "write") == end) {
    provided.push_back("write");
  }
  return IsSubset(required, provided);
}

}

CoreClient::CoreClient(const Builder& builder) :
  MessageSigner(builder.getSigningIdentity()),
  io_context(builder.getIoContext()), keysFilePath(builder.getKeysFilePath()),
  caCertFilepath(builder.getCaCertFilepath()),
  rootCAs(X509RootCertificates{ReadFile(builder.getCaCertFilepath())}),
  privateKeyData(builder.getPrivateKeyData()), publicKeyData(builder.getPublicKeyData()), privateKeyPseudonyms(builder.getPrivateKeyPseudonyms()),
  publicKeyPseudonyms(builder.getPublicKeyPseudonyms()),
  accessManagerEndPoint(builder.getAccessManagerEndPoint()),
  storageFacilityEndPoint(builder.getStorageFacilityEndPoint()),
  transcryptorEndPoint(builder.getTranscryptorEndPoint()) {

  clientAccessManager = tryConnectServerProxy<AccessManagerClient>(accessManagerEndPoint);
  clientStorageFacility = tryConnectServerProxy<StorageClient>(storageFacilityEndPoint);
  clientTranscryptor = tryConnectServerProxy<TranscryptorClient>(transcryptorEndPoint);

  if (keysFilePath.has_value()) {
    enrollmentSubject.get_observable().subscribe(
      [keysFilePath = *keysFilePath](const EnrollmentResult& result){
      LOG(LOG_TAG, debug) << "Writing new keys to " << keysFilePath;
      std::ofstream sf(keysFilePath.string());
      result.writeJsonTo(sf);
    });
  }
}

rxcpp::observable<PolymorphicPseudonym> CoreClient::parsePPorIdentity(const std::string& participantIdOrPP) {
  return this->parsePpsOrIdentities({ participantIdOrPP })
    .op(RxGetOne("set of polymorphic pseudonyms"))
    .map([](std::shared_ptr<std::vector<PolymorphicPseudonym>> pps) {
    assert(pps->size() == 1U);
    return pps->front();
      });
}

rxcpp::observable<std::shared_ptr<std::vector<PolymorphicPseudonym>>> CoreClient::parsePpsOrIdentities(const std::vector<std::string>& idsAndOrPps) {
  // Local pseudonyms and participant aliases must be looked up in the set of (decrypted) local pseudonyms for the access group
  auto ppsByLp = CreateRxCache([this]() {
    return this->getLocalizedPseudonyms()
      .reduce(
        std::make_shared<std::unordered_map<std::string, PolymorphicPseudonym>>(),
        [this](std::shared_ptr<std::unordered_map<std::string, PolymorphicPseudonym>> all, const LocalPseudonyms& entry) {
          auto decrypted = entry.mAccessGroup->decrypt(privateKeyPseudonyms);
          all->emplace(decrypted.text(), entry.mPolymorphic); // Don't assert that it's emplaced; we may be processing idsAndOrPps that refer to the same participant
          return all;
        }
      );
    });
  // Participant aliases (user pseudonyms) must have the format specified in global config
  auto userPseudFormat = CreateRxCache([this]() {
    return this->getGlobalConfiguration()
      .map([](std::shared_ptr<GlobalConfiguration> globalConfig) { return globalConfig->getUserPseudonymFormat(); });
    });

  auto results = std::make_shared<std::map<size_t, PolymorphicPseudonym>>(); // The entries that we'll (fill and) return
  auto entries = std::make_shared<std::vector<rxcpp::observable<std::pair<size_t, PolymorphicPseudonym>>>>(); // RX pipelines that'll produce index+PP pairs

  // Iterate over requested idsAndOrPps
  for (size_t i = 0U; i < idsAndOrPps.size(); ++i) {
    const auto& participantIdOrPP = idsAndOrPps[i];
    if (participantIdOrPP.size() == PolymorphicPseudonym::TextLength()) { // This is a text representation of a PP: add the (index+)PP to our "results" map immediately
      [[maybe_unused]] auto emplaced = results->emplace(i, PolymorphicPseudonym::FromText(participantIdOrPP)).second;
      assert(emplaced);
    }
    else if (participantIdOrPP.size() == LocalPseudonym::TextLength()) { // This is a local pseudonym: look it up in the (RX-provided) full set of local pseudonyms
      entries->emplace_back(ppsByLp->observe()
        .map([i, participantIdOrPP](std::shared_ptr<std::unordered_map<std::string, PolymorphicPseudonym>> ppsByLp) {
          auto position = ppsByLp->find(participantIdOrPP);
          if (position == ppsByLp->cend()) {
            throw std::runtime_error("Can't find local pseudonym " + participantIdOrPP);
          }
          return std::make_pair(i, position->second);
          }));
    }
    else { // We need the user pseudonym format to determine what type of ID we're dealing with
      entries->emplace_back(userPseudFormat->observe()
        .flat_map([this, i, participantIdOrPP, ppsByLp](const UserPseudonymFormat& userPseudFormat) -> rxcpp::observable<std::pair<size_t, PolymorphicPseudonym>> {
          if (userPseudFormat.matches(participantIdOrPP)) { // This is a participant alias (aka user pseudonym): look it up in the (RX-provided) full set of local pseudonyms
            auto pseudonymStart = userPseudFormat.stripPrefix(participantIdOrPP);
            return ppsByLp->observe()
              .map([i, participantIdOrPP, pseudonymStart](std::shared_ptr<std::unordered_map<std::string, PolymorphicPseudonym>> ppsByLp) {
              auto position = std::find_if(ppsByLp->cbegin(), ppsByLp->cend(), [pseudonymStart](const auto& pair) {return boost::starts_with(pair.first, pseudonymStart); });
              if (position == ppsByLp->cend()) {
                throw std::runtime_error("Can't find local pseudonym matching " + participantIdOrPP);
              }
              return std::make_pair(i, position->second);
                });
          }
          else if (participantIdOrPP.size() > 100) { // This is not a specification that we can parse
            throw std::invalid_argument("Too many characters in participant identifier");
          }
          else { // This is a participant identifier (PEP ID)
            return rxcpp::observable<>::just(std::make_pair(i, this->generateParticipantPolymorphicPseudonym(participantIdOrPP)));
          }
          }));
    }
  }

  // Produce index+PP pairs for idsAndOrPps that must be parsed using RX
  return RxIterate(entries)
    .flat_map([](rxcpp::observable<std::pair<size_t, PolymorphicPseudonym>> entry) { return entry.op(RxGetOne("parsed polymorphic pseudonym")); })
    .reduce(results, [](std::shared_ptr<std::map<size_t, PolymorphicPseudonym>> results, const auto& pair) { // Add those RX-provided index+PP pairs to our "results" map, filling in the blanks
    [[maybe_unused]] auto emplaced = results->emplace(pair.first, pair.second).second;
    assert(emplaced);
    return results;
      })
    .map([](std::shared_ptr<std::map<size_t, PolymorphicPseudonym>> all) { // We now have a complete set of results: convert map<index, PP> to a vector<PP>
    auto result = std::make_shared<std::vector<PolymorphicPseudonym>>();
    result->reserve(all->size());
    for (const auto& entry : *all) {
      assert(entry.first == result->size()); // Since our "results" map is ordered by index
      result->emplace_back(entry.second);
    }
    return result;
      });
}

PolymorphicPseudonym CoreClient::generateParticipantPolymorphicPseudonym(const std::string& participantSID) {
  return PolymorphicPseudonym::FromIdentifier(publicKeyPseudonyms, participantSID);
}


std::shared_ptr<CoreClient> CoreClient::OpenClient(const Configuration& config,
                                           std::shared_ptr<boost::asio::io_context> io_context,
                                           bool persistKeysFile) {
  Builder builder;
  builder.initialize(config, io_context, persistKeysFile);
  return builder.build();
}

void CoreClient::Builder::initialize(
    const Configuration& config,
    std::shared_ptr<boost::asio::io_context> io_context,
    bool persistKeysFile) {
  try {
    std::filesystem::path keysFile;
    std::optional<std::filesystem::path> shadowPublicKeyFile;

    try {
      // See #1797: the keys file must be (read from and) written to the cwd
      // because the config's directory may be read-only (e.g. on Windows installations).
      keysFile = std::filesystem::current_path() / config.get<std::string>("KeysFile");

      this->setCaCertFilepath(config.get<std::filesystem::path>("CACertificateFile"));
      this->setPublicKeyData(config.get<ElgamalPublicKey>("PublicKeyData"));
      this->setPublicKeyPseudonyms(config.get<ElgamalPublicKey>("PublicKeyPseudonyms"));

      if (auto amConfig = config.get<std::optional<EndPoint>>("AccessManager")) {
        this->setAccessManagerEndPoint(*amConfig);
      }

      if (auto tcConfig = config.get<std::optional<EndPoint>>("Transcryptor")) {
        this->setTranscryptorEndPoint(*tcConfig);
      }

      if (auto sfConfig = config.get<std::optional<EndPoint>>("StorageFacility")) {
        this->setStorageFacilityEndPoint(*sfConfig);
      }
    } catch (std::exception& e) {
      LOG(LOG_TAG, error) << "Error with configuration file: " << e.what();
      std::cerr << "Error with configuration file: " << e.what() << std::endl;
      throw;
    }

    if (persistKeysFile) {
      // Ensure that CoreClient writes future enrollment data to file...
      this->setKeysFilePath(keysFile);
      // ...and try to load previously persisted keys from it
      if (std::filesystem::exists(keysFile)) {
        Configuration keysConfig = Configuration::FromFile(keysFile);
        auto strEnrollmentScheme = keysConfig.get<std::optional<std::string>>("EnrollmentScheme");
        std::optional<EnrollmentScheme> enrollmentScheme;
        if (strEnrollmentScheme) {
          enrollmentScheme = Serialization::ParseEnum<EnrollmentScheme>(*strEnrollmentScheme);
        }
        if (enrollmentScheme && *enrollmentScheme == EnrollmentScheme::ENROLLMENT_SCHEME_CURRENT) {
          this->setPrivateKeyPseudonyms(ElgamalPrivateKey::FromText(keysConfig.get<std::string>("PseudonymKey")));
          this->setPrivateKeyData(ElgamalPrivateKey::FromText(keysConfig.get<std::string>("DataKey")));
          this->setSigningIdentity(std::make_shared<X509Identity>(
            AsymmetricKey(keysConfig.get<std::string>("PrivateKey")),
            X509CertificateChain(keysConfig.get<std::string>("CertificateChain"))));
        }
        else {
          LOG(LOG_TAG, info) << "Skipped loading keys file because it is from an older version";
        }
      }
      else {
        LOG(LOG_TAG, info) << "Skipped loading keys file because it does not exist";
      }
    }

    if (io_context == nullptr) {
      this->setIoContext(std::make_shared<boost::asio::io_context>());

      IoContextThread t(this->getIoContext());
      t.detach();
    } else {
      this->setIoContext(io_context);
    }
  } catch (std::exception& e) {
    LOG(LOG_TAG, error) << "Error with configuration file: " << e.what() << std::endl;
    std::cerr << "Error with configuration file: " << e.what() << std::endl;
    throw;
  }
}


rxcpp::observable<ColumnAccess> CoreClient::getAccessibleColumns(bool includeImplicitlyGranted,
                                                             const std::vector<std::string>& requireModes) {
  return clientAccessManager->requestColumnAccess(ColumnAccessRequest{includeImplicitlyGranted,
                                                                                         requireModes});
}

rxcpp::observable<std::string> CoreClient::getInaccessibleColumns(const std::string& mode, rxcpp::observable<std::string> columns) {
  return columns.op(RxToSet()) // Create a set of columns we still need to check
    .zip(this->getAccessibleColumns(true)) // Combine it with "the stuff we have access to"
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
    return rxcpp::observable<>::iterate(*remaining);
      });
}

rxcpp::observable<ParticipantGroupAccess> CoreClient::getAccessibleParticipantGroups(bool includeImplicitlyGranted) {
  return clientAccessManager->requestParticipantGroupAccess(ParticipantGroupAccessRequest{
      includeImplicitlyGranted});
}

rxcpp::observable<int> CoreClient::getRegistrationExpiryObservable() {
  return registrationSubject.get_observable();
}

std::shared_ptr<const StorageClient> CoreClient::getStorageClient(bool require) const {
  return GetConstServerProxy(clientStorageFacility, "Storage Facility", require);
}

std::shared_ptr<const TranscryptorClient> CoreClient::getTranscryptorClient(bool require) const {
  return GetConstServerProxy(clientTranscryptor, "Transcryptor", require);
}

std::shared_ptr<const AccessManagerClient> CoreClient::getAccessManagerClient(bool require) const {
  return GetConstServerProxy(clientAccessManager, "Access Manager", require);
}

const std::shared_ptr<boost::asio::io_context>& CoreClient::getIoContext() const {
  return io_context;
}

rxcpp::observable<FakeVoid> CoreClient::shutdown() {
  return rxcpp::observable<>::iterate(
    std::vector<rxcpp::observable<FakeVoid>> {
      clientAccessManager->shutdown(),
      clientStorageFacility->shutdown(),
      clientTranscryptor->shutdown()
    }
  ).merge().last();
}

rxcpp::observable<VerifiersResponse> CoreClient::getRSKVerifiers() {
  return clientAccessManager->requestVerifiers();
}

rxcpp::observable<std::shared_ptr<GlobalConfiguration>> CoreClient::getGlobalConfiguration() {
  if (mGlobalConf != nullptr) {
    return rxcpp::observable<>::just(mGlobalConf);
  }

  return clientAccessManager->requestGlobalConfiguration()
    .map([this](const GlobalConfiguration& gc) {return mGlobalConf = MakeSharedCopy(gc); });
}

rxcpp::observable<std::shared_ptr<ColumnNameMappings>> CoreClient::getColumnNameMappings() {
  return clientAccessManager->requestColumnNameMapping(ColumnNameMappingRequest{})
      .map([](const ColumnNameMappingResponse& response) {
        return std::make_shared<ColumnNameMappings>(response.mappings);
      });
}

rxcpp::observable<std::shared_ptr<ColumnNameMappings>> CoreClient::readColumnNameMapping(const ColumnNameSection&
                                                                                         original) {
  return clientAccessManager
      ->requestColumnNameMapping(ColumnNameMappingRequest{CrudAction::Read, original, std::nullopt})
      .map([](const ColumnNameMappingResponse& response) {
        return std::make_shared<ColumnNameMappings>(response.mappings);
      });
}

rxcpp::observable<std::shared_ptr<ColumnNameMappings>> CoreClient::createColumnNameMapping(const ColumnNameMapping&
                                                                                           mapping) {
  return clientAccessManager
      ->requestColumnNameMapping(ColumnNameMappingRequest{
          CrudAction::Create, mapping.original, mapping.mapped})
      .map([](const ColumnNameMappingResponse& response) {
        assert(response.mappings.size() == 1U);
        return std::make_shared<ColumnNameMappings>(response.mappings);
      });
}

rxcpp::observable<std::shared_ptr<ColumnNameMappings>> CoreClient::updateColumnNameMapping(const ColumnNameMapping&
                                                                                           mapping) {
  return clientAccessManager
      ->requestColumnNameMapping(ColumnNameMappingRequest{
          CrudAction::Update, mapping.original, mapping.mapped})
      .map([](const ColumnNameMappingResponse& response) {
        assert(response.mappings.size() == 1U);
        return std::make_shared<ColumnNameMappings>(response.mappings);
      });
}

rxcpp::observable<FakeVoid> CoreClient::deleteColumnNameMapping(const ColumnNameSection& original) {
  return clientAccessManager
      ->requestColumnNameMapping(
          ColumnNameMappingRequest{CrudAction::Delete, original, std::nullopt})
      .map([](const ColumnNameMappingResponse& response) { return FakeVoid(); });
}

rxcpp::observable<std::shared_ptr<StructureMetadataEntry>> CoreClient::getStructureMetadata(
    StructureMetadataType subjectType,
    std::vector<std::string> subjects,
    std::vector<StructureMetadataKey> keys) {
  return clientAccessManager
      ->requestStructureMetadata(StructureMetadataRequest{ subjectType, std::move(subjects), std::move(keys) })
      .map([](StructureMetadataEntry entry) { return MakeSharedCopy(std::move(entry)); });
}

rxcpp::observable<FakeVoid> CoreClient::setStructureMetadata(StructureMetadataType subjectType, MessageTail<StructureMetadataEntry> entries) {
  return clientAccessManager
      ->requestSetStructureMetadata(SetStructureMetadataRequest{subjectType}, std::move(entries))
      .map([](SetStructureMetadataResponse response) {
        return FakeVoid();
      });
}

rxcpp::observable<FakeVoid> CoreClient::removeStructureMetadata(StructureMetadataType subjectType, std::vector<StructureMetadataSubjectKey> subjectKeys) {
  return clientAccessManager
      ->requestSetStructureMetadata(SetStructureMetadataRequest{.subjectType = subjectType, .remove = std::move(subjectKeys)})
      .map([](SetStructureMetadataResponse) { return FakeVoid(); });
}

std::shared_ptr<WorkerPool> CoreClient::getWorkerPool() {
  if (mWorkerPool == nullptr)
    mWorkerPool = WorkerPool::getShared();
  return mWorkerPool;
}

rxcpp::observable<std::shared_ptr<std::vector<std::optional<PolymorphicPseudonym>>>> CoreClient::findPpsForShortPseudonyms(const std::vector<std::string>& sps, const std::optional<StudyContext>& studyContext) {
  auto allSps = std::make_shared<std::map<std::string, std::size_t, CaseInsensitiveCompare>>();
  for (size_t i = 0U; i < sps.size(); ++i) {
    [[maybe_unused]] auto emplaced = allSps->emplace(sps[i], i).second;
    assert(emplaced);
  }

  return this->getGlobalConfiguration()
    .flat_map([this, allSps, studyContext](std::shared_ptr<pep::GlobalConfiguration> gc) {
    auto columns = std::make_shared<std::set<std::string>>();
    for (const auto& pair : *allSps) {
      const auto& shortPseudonym = pair.first;
      auto definition = gc->getShortPseudonymForValue(shortPseudonym);
      if (!definition.has_value()) {
        throw ShortPseudonymFormatError(shortPseudonym);
      }
      if (studyContext && !studyContext->matchesShortPseudonym(*definition)) {
        throw ShortPseudonymContextError(shortPseudonym, studyContext->getId());
      }
      columns->emplace(definition->getColumn().getFullName());
    }

    return this->getAccessibleParticipantGroups(true)
      .flat_map([this, allSps, columns](const ParticipantGroupAccess& access) {
      pep::enumerateAndRetrieveData2Opts opts;
      for (const auto& [pg, modes] : access.participantGroups) {
        if (std::find(modes.begin(), modes.end(), "access") != modes.end()
          && std::find(modes.begin(), modes.end(), "enumerate") != modes.end()) {
          opts.groups.push_back(pg);
        }
      }
      if (opts.groups.empty()) {
        throw std::runtime_error("Cannot do shortpseudonym lookup. User does not have the appropriate access to any participant group");
      }
      std::copy(columns->cbegin(), columns->cend(), std::back_inserter(opts.columns));

      return this->enumerateAndRetrieveData2(opts);
        });
      })
    .reduce(
      std::make_shared<std::vector<std::optional<PolymorphicPseudonym>>>(allSps->size()),
      [allSps](std::shared_ptr<std::vector<std::optional<PolymorphicPseudonym>>> result, const EnumerateAndRetrieveResult& ear) {
        assert(ear.mDataSet);
        auto position = allSps->find(ear.mData);
        if (position != allSps->cend()) {
          auto index = position->second;
          (*result)[index] = ear.mLocalPseudonyms->mPolymorphic;
        }
        return result;
      });
}

rxcpp::observable<PolymorphicPseudonym> CoreClient::findPPforShortPseudonym(std::string shortPseudonym, const std::optional<StudyContext>& studyContext) {
  return this->findPpsForShortPseudonyms({ shortPseudonym }, studyContext)
    .map([shortPseudonym](std::shared_ptr<std::vector<std::optional<PolymorphicPseudonym>>> multiple) {
    assert(multiple->size() == 1U);
    auto result = multiple->front();
    if (!result.has_value()) {
      throw std::runtime_error("Short pseudonym " + shortPseudonym + " not found");
    }
    return *result;
      });
}

rxcpp::observable<LocalPseudonyms> CoreClient::getLocalizedPseudonyms()
{
  return getAccessibleParticipantGroups(true).flat_map([this](ParticipantGroupAccess participantGroupAccess) {
    requestTicket2Opts tOpts;
    tOpts.modes = { "read" };
    tOpts.includeAccessGroupPseudonyms = true;
    for (auto& [participantGroup, modes] : participantGroupAccess.participantGroups) {
      if (std::find(modes.begin(), modes.end(), "access") != modes.end()
          && std::find(modes.begin(), modes.end(), "enumerate") != modes.end()) {
        tOpts.participantGroups.push_back(participantGroup);
      }
    }
    return requestTicket2(tOpts);
  }).flat_map([this](IndexedTicket2 ticket) {
    return rxcpp::observable<>::iterate(ticket.getTicket()->open(rootCAs, getEnrolledGroup()).mPseudonyms);
  });

}

rxcpp::observable<IndexedTicket2> CoreClient::requestTicket2(const requestTicket2Opts& opts) {
  LOG(LOG_TAG, debug) << "requestTicket";

  if (opts.ticket != nullptr && ModesInclude(opts.modes, opts.ticket->getModes())
      && IsSubset(opts.participantGroups, opts.ticket->getParticipantGroups())
      && IsSubset(opts.columnGroups, opts.ticket->getColumnGroups())
      && IsSubset(opts.pps, opts.ticket->getPolymorphicPseudonyms())
      && IsSubset(opts.columns, opts.ticket->getColumns())) {
    return rxcpp::observable<>::just(*opts.ticket);
  }
  if (opts.forceTicket) {
    return rxcpp::observable<>::error<IndexedTicket2>(std::runtime_error("Query out of scope of provided Ticket"));
  }
  assert(ContainsUniqueValues(opts.pps));
  return clientAccessManager->requestIndexedTicket(sign(TicketRequest2{
      .mModes = opts.modes,
      .mParticipantGroups = opts.participantGroups,
      .mPolymorphicPseudonyms = opts.pps,
      .mColumnGroups = opts.columnGroups,
      .mColumns = opts.columns,
      .mRequestIndexedTicket = true,
      .mIncludeUserGroupPseudonyms = opts.includeAccessGroupPseudonyms}));
}


}
