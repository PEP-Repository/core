#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/registrationserver/RegistrationServer.hpp>
#include <pep/async/RxCartesianProduct.hpp>
#include <pep/async/RxEnsureProgress.hpp>
#include <pep/structure/ShortPseudonyms.hpp>
#include <pep/auth/FacilityType.hpp>
#include <pep/registrationserver/RegistrationServerSerializers.hpp>
#include <pep/networking/EndPoint.PropertySerializer.hpp>
#include <pep/elgamal/CurvePoint.PropertySerializer.hpp>
#include <pep/core-client/CoreClient.hpp>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-on_error_resume_next.hpp>
#include <rxcpp/operators/rx-switch_if_empty.hpp>

#include <iostream>

#ifdef WITH_CASTOR
#include <pep/castor/Participant.hpp>
#include <pep/castor/ImportColumnNamer.hpp>
#endif

namespace pep {

namespace {

const std::string LOG_TAG("RegistrationServer");

#ifdef WITH_CASTOR

rxcpp::observable<std::shared_ptr<castor::Study>> LoadCastorStudies(rxcpp::observable<std::shared_ptr<castor::Study>> allStudies, rxcpp::observable<ShortPseudonymDefinition> sps) {
  auto abbrevsBySlug = std::make_shared<std::unordered_map<std::string, std::string>>();

  return allStudies
    .op(RxToUnorderedMap([](std::shared_ptr<castor::Study> study) {return study->getSlug(); }))
    .flat_map([sps, abbrevsBySlug](std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<castor::Study>>> studiesBySlug) {
    return sps
      .filter([](const ShortPseudonymDefinition& sp) {return sp.getCastor(); })
      .map([](const ShortPseudonymDefinition& sp) {return *sp.getCastor(); })
      .map([studiesBySlug, abbrevsBySlug](const CastorShortPseudonymDefinition& castorSp) {
      auto slug = castorSp.getStudySlug();
      auto position = studiesBySlug->find(slug);
      if (position == studiesBySlug->cend()) {
        LOG(LOG_TAG, error) << "Study " << slug << " has not been loaded from Castor";
        return std::shared_ptr<castor::Study>();
      }

      LOG(LOG_TAG, debug) << "Study " << slug << " has been loaded from Castor";
      auto study = position->second;
      auto abbrev = castorSp.getSiteAbbreviation();

      auto assigned = abbrevsBySlug->find(slug);
      if (assigned != abbrevsBySlug->cend()) {
        // This study (slug) is referenced from multiple SP definitions, and a default site has already been assigned.
        assert(assigned->second == abbrev);
        return std::shared_ptr<castor::Study>(); // Prevent our result from emitting the same study multiple times.
      }

      // At this point, we know that this is the first (and perhaps only) SP definition referencing this study (slug). Set default site now and include the study in the result.
      study->setDefaultSiteByAbbreviation(abbrev);
      abbrevsBySlug->emplace(std::make_pair(slug, abbrev));
      return study;
    })
      .filter([](std::shared_ptr<castor::Study> study) {return study != nullptr; });
  });
}
#endif

rxcpp::observable<ShortPseudonymDefinition> GetShortPseudonymDefinitions(std::shared_ptr<RxCache<std::shared_ptr<GlobalConfiguration>>> globalConfiguration) {
  if (globalConfiguration == nullptr) {
    throw std::runtime_error("Cannot get short pseudonym definitions without global configuration");
  }
  return globalConfiguration->observe()
    .flat_map([](std::shared_ptr<GlobalConfiguration> config) {return rxcpp::observable<>::iterate(config->getShortPseudonyms()); });
}

int ParseSqliteSelectCountResult(void *pArg, int argc, char **argv, char **columnNames) {
  assert(pArg != nullptr);
  assert(argc == 1);
  *static_cast<size_t*>(pArg) = strtoul(argv[0], nullptr, 10);
  return 0;
}

}

class RegistrationServer::ShortPseudonymCache : public std::enable_shared_from_this<ShortPseudonymCache> {
  friend class SharedConstructor<ShortPseudonymCache>;

private:
  std::shared_ptr<RxCache<std::string>> mRx;
  std::vector<std::string> mLocal;

private:
  ShortPseudonymCache(RegistrationServer& server, const std::filesystem::path& shadowStorageFile)
    : mRx(CreateRxCache([&server, shadowStorageFile]() {return server.initPseudonymStorage(shadowStorageFile); })) {
  }

public:
  void add(const std::string& localValue) { mLocal.push_back(localValue); }
  rxcpp::observable<std::string> observe() const { return mRx->observe().concat(rxcpp::observable<>::iterate(mLocal)); }

  static std::shared_ptr<ShortPseudonymCache> Create(RegistrationServer& server, const std::filesystem::path& shadowStorageFile) {
    auto result = std::shared_ptr<ShortPseudonymCache>(new ShortPseudonymCache(server, shadowStorageFile));
    result->mRx->observe().subscribe( // Ensure cache is populated immediately
      [](const std::string&) {},
      [](std::exception_ptr) {} // Ignore errors during preloading: just let the cache recover when it is re-observed
    );
    return result;
  }
};

RegistrationServer::Parameters::Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
  : SigningServer::Parameters(io_context, config) {
  std::filesystem::path keysFile;

  std::filesystem::path shadowPublicKeyFile;

  CoreClient::Builder clientBuilder;

  try {
    keysFile = config.get<std::filesystem::path>("KeysFile");
    clientBuilder.setPublicKeyData(config.get<ElgamalPublicKey>("PublicKeyData"));
    clientBuilder.setPublicKeyPseudonyms(config.get<ElgamalPublicKey>("PublicKeyPseudonyms"));

    setShadowStorageFile(config.get<std::filesystem::path>("ShadowStorageFile"));
    shadowPublicKeyFile = config.get<std::filesystem::path>("ShadowPublicKeyFile");

    clientBuilder.setAccessManagerEndPoint(config.get<EndPoint>("AccessManager"));
    clientBuilder.setStorageFacilityEndPoint(config.get<EndPoint>("StorageFacility"));
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with configuration file: " << e.what();
    throw;
  }

  setShadowPublicKey(AsymmetricKey(ReadFile(shadowPublicKeyFile)));

  std::string strPseudonymKey, strDataKey;
  try {
    Configuration keysConfig = Configuration::FromFile(keysFile);
    strPseudonymKey = boost::algorithm::unhex(keysConfig.get<std::string>("PseudonymKey"));
    strDataKey = boost::algorithm::unhex(keysConfig.get<std::string>("DataKey"));
  }
  catch (std::exception& e) {
    LOG(LOG_TAG, critical) << "Error with keys file: " << keysFile << " : " << e.what();
    throw;
  }

  clientBuilder.setIoContext(getIoContext())
    .setCaCertFilepath(getRootCACertificatesFilePath())
    .setSigningIdentity(getSigningIdentity())
    .setPrivateKeyData(ElgamalPrivateKey(strDataKey))
    .setPrivateKeyPseudonyms(ElgamalPrivateKey(strPseudonymKey));
  std::shared_ptr<CoreClient> client = clientBuilder.build();

  setClient(client);

#ifdef WITH_CASTOR
  auto castorAPIKeyFile = config.get<std::optional<std::filesystem::path>>("Castor.APIKeyFile");
  if (!castorAPIKeyFile) {
    LOG(LOG_TAG, info) << "No Castor.APIKeyFile configured: attempts to access the Castor API will fail.";
  }
  else {
    if(std::filesystem::exists(castorAPIKeyFile.value()))
    {
      auto castor = castor::CastorConnection::Create(*castorAPIKeyFile, this->getIoContext());
      setCastorConnection(castor);
    }
    else {
      LOG(LOG_TAG, warning) << "CastorAPIKey.json is not found at specified directory: attempts to access the Castor API will fail.";
    }
  }
#endif
}

/*!
  * \return PEP client used to store data
  */
std::shared_ptr<CoreClient> RegistrationServer::Parameters::getClient() const {
  return client;
}
/*!
  * \param client PEP client used to store data
  */
void RegistrationServer::Parameters::setClient(const std::shared_ptr<CoreClient> client) {
  Parameters::client = client;
}

/*!
  * \return Path to the shadow storage file
  */
const std::filesystem::path& RegistrationServer::Parameters::getShadowStorageFile() const {
  return shadowStorageFile;
}
/*!
  * \param shadowStorageFile Path to the shadow storage file
  */
void RegistrationServer::Parameters::setShadowStorageFile(const std::filesystem::path& shadowStorageFile) {
  Parameters::shadowStorageFile = std::filesystem::weakly_canonical(shadowStorageFile);
}

/*!
  * \return Public key of the shadow storage
  */
const AsymmetricKey& RegistrationServer::Parameters::getShadowPublicKey() const {
  return shadowPublicKey;
}
/*!
  * \param shadowPublicKey Public key of the shadow storage
  */
void RegistrationServer::Parameters::setShadowPublicKey(const AsymmetricKey& shadowPublicKey) {
  Parameters::shadowPublicKey = shadowPublicKey;
}

#ifdef WITH_CASTOR
/*!
  * \return The Castor client connection
  */
std::shared_ptr<castor::CastorConnection> RegistrationServer::Parameters::getCastorConnection() const {
  return castorConnection;
}
/*!
  * \param castorConnection The Castor client connection
  */
void RegistrationServer::Parameters::setCastorConnection(std::shared_ptr <castor::CastorConnection> castorConnection) {
  Parameters::castorConnection = castorConnection;
}
#endif

void RegistrationServer::Parameters::check() const {
  if(!client)
    throw std::runtime_error("client must be set");
  if(shadowStorageFile.empty())
    throw std::runtime_error("shadowStorageFile must not be empty");
  if(!shadowPublicKey.isSet())
    throw std::runtime_error("shadowPublicKey must be set");
  if (GetFacilityType(this->getSigningIdentity()->getCertificateChain()) != FacilityType::RegistrationServer)
    throw std::runtime_error("Invalid certificate chain for Registration Server");
  SigningServer::Parameters::check();
}


bool RegistrationServer::openDatabase(const std::filesystem::path& file) {
  auto result = !std::filesystem::exists(file);
  int err;

  try {
    // Open SQLite database for shadow administration of identifiers/short pseudonyms
    err = sqlite3_open(file.string().c_str(), &pShadowStorage);
    if (err != SQLITE_OK) {
      LOG(LOG_TAG, warning) << "Error opening SQLite database: " << err;
      throw std::runtime_error("Error opening SQLite database");
    }

    // Create table if it does not exist yet
    err = sqlite3_exec(pShadowStorage, "CREATE TABLE IF NOT EXISTS `ShadowShortPseudonyms` (`EncryptedIdentifier`  BLOB, `EncryptedShortPseudonym`  BLOB);", nullptr, nullptr, nullptr);
    if (err != SQLITE_OK) {
      LOG(LOG_TAG, warning) << "Error creating SQLite table: " << err;
      throw std::runtime_error("Error creating SQLite table");
    }

    // There are two versions of the database schemas.  In the second version
    // we have an Id field on ShortPseudonyms.
    sqlite3_stmt* colStmt;
    err = sqlite3_prepare_v2(pShadowStorage,
      "PRAGMA table_info(ShadowShortPseudonyms);",
      -1, &colStmt, nullptr);
    if (err != SQLITE_OK || colStmt == nullptr) {
      throw std::runtime_error("Error on PRAGMA table_info");
    }
    bool upgradeToSchema2 = true;
    while (sqlite3_step(colStmt) == SQLITE_ROW) {
      if (strcmp(reinterpret_cast<const char*>(sqlite3_column_text(colStmt, 1)), "Id") == 0) {
        upgradeToSchema2 = false;
        break;
      }
    }
    sqlite3_finalize(colStmt);

    // Add schema 2 columns
    if (upgradeToSchema2) {
      LOG(LOG_TAG, info) << "Adding Id field to ShadowShortPseudonyms";
      err = sqlite3_exec(pShadowStorage,
        "BEGIN TRANSACTION; \
              CREATE TABLE ShadowShortPseudonyms_new( \
                  `EncryptedIdentifier` BLOB, \
                  `EncryptedShortPseudonym` BLOB, \
                  `Id` INTEGER PRIMARY KEY AUTOINCREMENT); \
              INSERT INTO ShadowShortPseudonyms_new( \
                  `EncryptedIdentifier`, \
                  `EncryptedShortPseudonym`) \
                SELECT `EncryptedIdentifier`, \
                      `EncryptedShortPseudonym` \
                  FROM ShadowShortPseudonyms; \
              DROP TABLE ShadowShortPseudonyms; \
              ALTER TABLE ShadowShortPseudonyms_new RENAME TO ShadowShortPseudonyms; \
              COMMIT;", nullptr, nullptr, nullptr);
      if (err != SQLITE_OK) {
        LOG(LOG_TAG, warning) << "Adding Id columns failed:" << sqlite3_errmsg(pShadowStorage);
        throw std::runtime_error("Error adding Id column");
      }
    }

    return result;
  }
  catch (...) {
    /* From https://www.sqlite.org/c3ref/open.html :
     * Whether or not an error occurs when it is opened, resources associated with the database connection handle
     * should be released by passing it to sqlite3_close() when it is no longer required.
    */
    closeDatabase();

    // if we created the file, remove it again so the next invocation will know to initialize the DB contents.
    if (result && std::filesystem::exists(file)) {
      std::filesystem::remove(file);
    }
    throw;
  }
}

void RegistrationServer::closeDatabase() noexcept {
  if (pShadowStorage) {
    if (sqlite3_close_v2(pShadowStorage) != SQLITE_OK) {
      LOG(LOG_TAG, error) << "Failed to close shadow storage database";
    }
    else {
      pShadowStorage = nullptr;
    }
  }
}

rxcpp::observable<std::string> RegistrationServer::initPseudonymStorage(const std::filesystem::path& shadowStorageFile) {
  using Pseudonyms = std::unordered_map<std::string, std::string>;
  using PseudonymsByPp = std::unordered_map<PolymorphicPseudonym, Pseudonyms>;

  return RxEnsureProgress(*pClient->getIoContext(), "Pseudonym storage initialization", [this, shadowStorageFile](std::shared_ptr<ActivityMonitor> monitor) {
    auto rebuild = openDatabase(shadowStorageFile); // If database was (re)created, fill it with existing SPs (retrieved from Storage Facility)
    auto count = std::make_shared<size_t>(0U);

    return getShortPseudonymDefinitions()
      .op(RxRecordActivity(monitor, "retrieving short pseudonym definitions"))
      .map([](ShortPseudonymDefinition def) {return def.getColumn().getFullName(); }) // Convert to observable<> of SP column names
      .concat(rxcpp::observable<>::just(std::string("ParticipantIdentifier"))) // Add column name for (pseudonym-like) participant identifier
      .op(RxToVector()) // Aggregate column names into a vector<std::string>
      .flat_map([client = pClient](std::shared_ptr<std::vector<std::string>> columns) { // Retrieve data for all pseudonym-containing columns
      pep::enumerateAndRetrieveData2Opts opts;
      opts.groups = { "*" };
      opts.columns = *columns;
      opts.columnGroups = { "ShortPseudonyms" };
      return client->enumerateAndRetrieveData2(opts);
    })
      .op(RxRecordActivity(monitor, "retrieving pseudonym values"))
      .reduce( // Group pseudonyms by participant (PP)
        std::make_shared<PseudonymsByPp>(),
        [](std::shared_ptr<PseudonymsByPp> pps, const EnumerateAndRetrieveResult& result) {
      if (!result.mDataSet) { // TODO: support this
        throw std::runtime_error("Storage Facility did not return pseudonym as inline data for column " + result.mColumn + " and participant " + result.mLocalPseudonyms->mPolymorphic.text());
      }
      (*pps)[result.mLocalPseudonyms->mPolymorphic][result.mColumn] = result.mData;
      return pps;
    })
      .flat_map([rebuild](std::shared_ptr<PseudonymsByPp> pps) { // Convert single unordered_map<> to observable<participant-data>
      if (rebuild) {
        LOG(LOG_TAG, info) << "Initializing shadow storage with short pseudonyms retrieved from Storage Facility";
      }
      return RxIterate(pps);
    })
      .flat_map([this, rebuild, count](const PseudonymsByPp::value_type& ppAndPseudonyms) { // Process each participant
      const auto& pseudonyms = ppAndPseudonyms.second;
      auto idPosition = pseudonyms.find("ParticipantIdentifier");
      auto end = pseudonyms.cend();
      if (idPosition == end) { // Cannot store without "ParticipantIdentifier" value
        if (rebuild) {
          LOG(LOG_TAG, warning) << "Cannot shadow store SPs: no ID found for participant " << ppAndPseudonyms.first.text();
        }
      }
      else { // Store this participant's short pseudonyms in the shadow DB
        const auto& id = idPosition->second;
        auto encryptedId = this->shadowPublicKey.encrypt(id);
        for (auto i = pseudonyms.cbegin(); i != end; ++i) {
          if (i != idPosition) { // Don't store the participant ID itself
            if (rebuild) {
              this->storeShortPseudonymShadow(encryptedId, i->first, i->second);
            }
            ++(*count);
          }
        }
      }

      return rxcpp::observable<>::iterate(pseudonyms)
        .map([](const Pseudonyms::value_type& columnAndValue) {return columnAndValue.second; });
    })
      .as_dynamic()
      .op(RxBeforeTermination([this, rebuild, count, shadowStorageFile](std::optional<std::exception_ptr> ep) {
      if (rebuild) {
        if (ep) {
          LOG(LOG_TAG, error) << "Shadow storage initialization failed after storage of " << *count << " entries: " << GetExceptionMessage(*ep);
          // Since we're rebuilding, the DB file was just created. Remove it so we can retry.
          this->closeDatabase();
          std::filesystem::remove(shadowStorageFile);
          LOG(LOG_TAG, info) << "Removed shadow storage database file to allow contents to be rebuilt next time";
        }
        else {
          LOG(LOG_TAG, info) << "Shadow storage initialized with " << *count << " entries";
        }
      }
      else {
        auto stored = this->countShadowStoredEntries();
        if (stored != *count) {
          LOG(LOG_TAG, warning) << "Expected " << *count << " shadow storage entries but found " << stored;
        }
      }
    }));
  });
}

size_t RegistrationServer::countShadowStoredEntries() const {
  size_t result;
  auto err = sqlite3_exec(pShadowStorage, "select count(*) from ShadowShortPseudonyms", &ParseSqliteSelectCountResult, &result, nullptr);
  if (err != SQLITE_OK) {
    LOG(LOG_TAG, warning) << "Error counting shadow storage entries: " << err;
    throw std::runtime_error("Error counting shadow storage entries");
  }
  return result;
}

  /*! \brief constructor for registration server
   *
   * \param parameters Parameters for a RegistrationServer::Connection
   */
RegistrationServer::RegistrationServer(std::shared_ptr<Parameters> parameters)
  : SigningServer(parameters),
  pClient(parameters->getClient()),
  shadowPublicKey(parameters->getShadowPublicKey()),
  mGlobalConfiguration(CreateRxCache([client = pClient]() {return RxEnsureProgress(*client->getIoContext(), "Global configuration retrieval", client->getGlobalConfiguration()); })),
  mShortPseudonyms(ShortPseudonymCache::Create(*this, parameters->getShadowStorageFile())) // cannot get a shared_ptr<RegistrationServer> during construction
#ifdef WITH_CASTOR
  , mCastorConnection(parameters->getCastorConnection())
  , mCastorStudies(CreateRxCache([io_context = pClient->getIoContext(), connection = mCastorConnection, config = mGlobalConfiguration]() -> rxcpp::observable<std::shared_ptr<castor::Study>> {
  if (connection == nullptr) {
    return rxcpp::observable<>::error<std::shared_ptr<castor::Study>>(std::runtime_error("Castor studies cannot be retrieved because connection has not been initialized"));
  }
  return RxEnsureProgress(*io_context, "Castor study loading", LoadCastorStudies(connection->getStudies(), GetShortPseudonymDefinitions(config))); }))
#endif
{
  RegisterRequestHandlers(*this,
                          &RegistrationServer::handleSignedRegistrationRequest,
                          &RegistrationServer::handleSignedPEPIdRegistrationRequest,
                          &RegistrationServer::handleListCastorImportColumnsRequest);
}

RegistrationServer::~RegistrationServer() {
  closeDatabase();
}

struct RegistrationContext {
  std::string encryptedIdentifier;
  std::shared_ptr<PolymorphicPseudonym> pp;
};

/*!
  * \brief Store the tag and short pseudonym encrypted in the shadow SQLite database together with the encrypted identifier. It returns the SQLite return code for the query.
  *
  * \param encryptedIdentifier The encrypted identifier provided by the client
  * \param tag The tag of short pseudonym to be stored
  * \param shortPseudonym The short pseudonym to be stored
  */
void RegistrationServer::storeShortPseudonymShadow(const std::string& encryptedIdentifier, const std::string& tag, const std::string& shortPseudonym) {
  sqlite3_stmt* insertStmt;

  std::string encryptedShortPseudonym = shadowPublicKey.encrypt(tag + ":" + shortPseudonym);

  //TODO Do we want to re-use the prepared statement?
  if (sqlite3_prepare_v2(pShadowStorage, "INSERT INTO ShadowShortPseudonyms(EncryptedIdentifier, EncryptedShortPseudonym) VALUES(?, ?)", -1, &insertStmt, nullptr) != SQLITE_OK) {
    LOG(LOG_TAG, warning) << "Error occured: " << sqlite3_errmsg(pShadowStorage);
    throw std::runtime_error("Error occured");
  }

  sqlite3_bind_blob(
    insertStmt,
    1,
    encryptedIdentifier.data(),
    static_cast<int>(encryptedIdentifier.size()),
    SQLITE_STATIC
  );
  sqlite3_bind_blob(
    insertStmt,
    2,
    encryptedShortPseudonym.data(),
    static_cast<int>(encryptedShortPseudonym.size()),
    SQLITE_STATIC
  );
  sqlite3_step(insertStmt);

  if (sqlite3_finalize(insertStmt) != SQLITE_OK) {
    LOG(LOG_TAG, warning) << "Error occured while storing in shadow administration: " << sqlite3_errmsg(pShadowStorage);
    throw std::runtime_error("Error occured while storing in shadow administration");
  }
}

rxcpp::observable<std::string> RegistrationServer::generatePseudonym(std::string prefix, int len) {
  auto sp = GenerateShortPseudonym(prefix, len);
  return mShortPseudonyms->observe()
    .map([sp](std::string existing) {return sp == existing; }) // Compare generated SP to each existing one
    .filter([](bool exists) {return exists; }) // Only return a (TRUE) value if we generated a duplicate
    .concat(rxcpp::observable<>::just(false)) // Append a default FALSE value: no, we didn't generate a duplicate
    .first() // Determine if we generated a duplicate: either the initial TRUE if we generated it, or the trailing (default) FALSE if we didn't generate it
    .flat_map([self = SharedFrom(*this), prefix, len, sp](bool duplicate) -> rxcpp::observable<std::string> { // Either return the generated SP or generate a new one
    if (duplicate) {
      return self->generatePseudonym(prefix, len);
    } else {
      self->mShortPseudonyms->add(sp);
      return rxcpp::observable<>::just(sp).as_dynamic();
    }
  })
    ;
}

rxcpp::observable<ShortPseudonymDefinition> RegistrationServer::getShortPseudonymDefinitions() const {
  return RxEnsureProgress(*pClient->getIoContext(), "Short pseudonym definition retrieval", GetShortPseudonymDefinitions(mGlobalConfiguration));
}

#ifdef WITH_CASTOR
std::shared_ptr<castor::CastorConnection> RegistrationServer::getCastorConnection() const {
  if (mCastorConnection == nullptr) {
    throw std::runtime_error("Castor connection is not available because it has not been initialized");
  }
  return mCastorConnection;
}

rxcpp::observable<std::shared_ptr<castor::Participant>> RegistrationServer::storeShortPseudonymInCastor(std::shared_ptr<castor::Study> study, ShortPseudonymDefinition definition) {
  return this->generatePseudonym(definition.getPrefix(), static_cast<int>(definition.getLength()))
    .flat_map([study](std::string sp) {return study->createParticipant(sp); })
    .on_error_resume_next(
      [self = SharedFrom(*this), study, definition](std::exception_ptr ep) -> rxcpp::observable<std::shared_ptr<castor::Participant>> {
    try {
      std::rethrow_exception(ep);
    }
    catch (const castor::CastorException& ex) {
      if (ex.status == castor::CastorConnection::RECORD_EXISTS) {
        LOG(LOG_TAG, info) << "Participant exists. Retrying with a different participant ID";
        return self->storeShortPseudonymInCastor(study, definition);
      }
      else {
        LOG(LOG_TAG, error) << "Castor Error: " << ex.what();
        throw;
      }
    }
    catch (const std::exception& ex) {
      LOG(LOG_TAG, error) << "Castor Error: " << ex.what();
      throw;
    }
  });
}
#endif

std::vector<std::string> RegistrationServer::getChecksumChainNames() const {
  return {"shadow-short-pseudonyms"};
}

void RegistrationServer::computeChecksumChainChecksum(
    const std::string& chain, std::optional<uint64_t> maxCheckpoint,
    uint64_t& checksum, uint64_t& checkpoint) {
  checksum = 0;
  checkpoint = 0;

  if (!maxCheckpoint)
    maxCheckpoint = std::numeric_limits<int64_t>::max();

  sqlite3_stmt* stmt;

  if (chain == "shadow-short-pseudonyms") {
    sqlite3_prepare_v2(pShadowStorage,
                        "SELECT \
            `Id`, \
            `EncryptedShortPseudonym`, \
            `EncryptedIdentifier` \
          FROM ShadowShortPseudonyms \
          Where Id <= ?;",
                        -1, &stmt, nullptr);
  } else {
    throw Error("Unknown checksumchain");
  }
  sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(*maxCheckpoint));

  int ncols = -1;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (ncols == -1)
      ncols = sqlite3_column_count(stmt);
    checkpoint = std::max(checkpoint, static_cast<uint64_t>(sqlite3_column_int64(stmt, 0)));

    Sha256 sha;

    for (int i = 0; i < ncols; i++) {
      switch (sqlite3_column_type(stmt, i)) {
      case SQLITE_BLOB:
        sha.update(
          static_cast<const uint8_t*>(sqlite3_column_blob(stmt, i)),
          static_cast<size_t>(sqlite3_column_bytes(stmt, i))
        );
        break;
      case SQLITE_TEXT:
        sha.update(
          static_cast<const uint8_t*>(sqlite3_column_text(stmt, i)),
          static_cast<size_t>(sqlite3_column_bytes(stmt, i))
        );
        break;
      default:
        break;
      }
    }

    auto digest = sha.digest();
    checksum ^= UnpackUint64BE(digest);
  }
  sqlite3_finalize(stmt);
}

messaging::MessageBatches RegistrationServer::handleSignedPEPIdRegistrationRequest(std::shared_ptr<SignedPEPIdRegistrationRequest> signedRequest) {
  signedRequest->validate(this->getRootCAs());

  struct ParticipantIdentity {
    std::string id;
    PolymorphicPseudonym pp;
  };

  auto server = SharedFrom(*this);
  return this->mGlobalConfiguration->observe() // Get global configuration
    .flat_map([server](std::shared_ptr<GlobalConfiguration> config) { // Generate a new PEP ID
    auto format = config->getGeneratedParticipantIdentifierFormat();
    assert(format.getNumberOfGeneratedDigits() <= static_cast<unsigned>(std::numeric_limits<int>::max()));

    return server->generatePseudonym(format.getPrefix(), static_cast<int>(format.getNumberOfGeneratedDigits()));
  })
    .map([server](std::string id) { // Produce a PP for the newly generated PEP ID
    auto pp = server->pClient->generateParticipantPolymorphicPseudonym(id);
    return MakeSharedCopy(ParticipantIdentity{ id, pp });
  }).flat_map([server](std::shared_ptr<ParticipantIdentity> participant) { // Raise an error if the generated (ID+)PP already existed
    return server->pClient->enumerateData2( // Check with Storage Facility if (the row for) the generated ID already exists
      {},                        // groups
      { participant->pp },          // pps
      {},                        // columnGroups
      { "ParticipantIdentifier" }) // columns
      .map([](const std::vector<EnumerateResult>& result) { // Raise an exception if (the row for) our generated ID already existed
      if (!result.empty()) {
        throw Error("Generated a duplicate participant ID. Please retry"); // TODO retry automatically instead
      }
      return FakeVoid();
        })
      .op(RxInstead(participant)); // No exception was raised, so the generated ID isn't a duplicate: return it for further processing
  })
    .flat_map([server](std::shared_ptr<ParticipantIdentity> participant) { // Not a duplicate ID: store it
    auto response = MakeSharedCopy(Serialization::ToString(PEPIdRegistrationResponse{ participant->id }));
    return server->pClient->storeData2(participant->pp, "ParticipantIdentifier",
      MakeSharedCopy(participant->id), { MetadataXEntry::MakeFileExtension(".txt") })
      .op(RxInstead(rxcpp::observable<>::from(response).as_dynamic()));
    });
}

messaging::MessageBatches RegistrationServer::handleSignedRegistrationRequest(std::shared_ptr<SignedRegistrationRequest> signedRequest) {
  auto request = signedRequest->open(this->getRootCAs());

  if (request.mEncryptionPublicKeyPem.empty()) {
    throw std::runtime_error("Participant registration requires the encryption key for shadow storage to be verified. Please ensure that the client provides one.");
  }
  if (shadowPublicKey != AsymmetricKey(request.mEncryptionPublicKeyPem)) {
    throw std::runtime_error("Cannot store short pseudonyms because client uses a different encryption key for shadow storage. Please ensure that client and server configurations match.");
  }

  std::shared_ptr<RegistrationContext> ctx = std::make_shared<RegistrationContext>();
  ctx->pp = std::make_shared<PolymorphicPseudonym>(request.mPolymorphicPseudonym);
  ctx->encryptedIdentifier = request.mEncryptedIdentifier;

  auto first_error = std::make_shared<std::exception_ptr>();
  auto reauthenticated = std::make_shared<bool>(false);

  struct ShortPseudonymEntry {
    StoreData2Entry store;
    std::string sp;

    ShortPseudonymEntry(std::shared_ptr<PolymorphicPseudonym> pp, const std::string& column, const std::string& sp)
      : store(pp, column, std::make_shared<std::string>(sp), {MetadataXEntry::MakeFileExtension(".txt")}), sp(sp) {
    }
  };

  auto server = SharedFrom(*this);
  return pClient->enumerateData2( // Get previously stored SPs for this participant
      {},                           // groups
      { *ctx->pp },                 // pps
      { "ShortPseudonyms" },        // columnGroups
      {})                           // columns
    .flat_map([](std::vector<EnumerateResult> results) { return rxcpp::observable<>::iterate(std::move(results)); }) // Convert observable<vector<EnumerateResult>> to observable<EnumerateResult>
    .map([](const EnumerateResult& result) {return result.mMetadata.getTag(); }) // Extract the column name
    .op(RxToVector()) // Convert to a single vector<> containing column names
    .op(RxCartesianProduct(getShortPseudonymDefinitions())) // Combine participant SPs with defined SPs
    .filter([](std::pair<std::shared_ptr<std::vector<std::string>>, ShortPseudonymDefinition> pair) { // Keep only defined SPs that the participant does not have
    auto end = pair.first->cend();
    return std::find(pair.first->cbegin(), end, pair.second.getColumn().getFullName()) == end;
  })
    .map([server, reauthenticated](std::pair< std::shared_ptr<std::vector<std::string>>, ShortPseudonymDefinition> pair) { // Keep only the ShortPseudonymDefinition
  #ifdef WITH_CASTOR
    // Reauthenticate if we're going to store a Castor SP
    if (pair.second.getCastor() && !*reauthenticated) {
      *reauthenticated = true;
      server->getCastorConnection()->reauthenticate();
    }
  #endif
    return pair.second;
  })
    .flat_map([server, ctx, first_error](ShortPseudonymDefinition unstored)->rxcpp::observable<ShortPseudonymEntry> { // Generate an SP for each previously unstored ShortPseudonymDefinition.mColumn
    rxcpp::observable<ShortPseudonymEntry> observable;
#ifdef WITH_CASTOR
    auto castor = unstored.getCastor();
    if (castor) { // Store participant in Castor
      auto slug = castor->getStudySlug();
      observable = server->mCastorStudies->observe() // Get all Castor studies
        .filter([slug](std::shared_ptr<castor::Study> candidate) {return candidate->getSlug() == slug; }) // Limit to the (one) study matching the SP to store
        .default_if_empty(std::shared_ptr<castor::Study>()) // Create a sentry (nullptr) value in case the study wasn't loaded
        .op(RxGetOne("studies with slug " + slug))
        .filter([unstored](std::shared_ptr<castor::Study> study) { // Issue a log message if the SP cannot be stored, and filter out the sentry (nullptr) value
        if (study == nullptr) {
          LOG(LOG_TAG, warning) << "Couldn't create Castor participant for " << unstored.getColumn().getFullName() << " because study " << unstored.getCastor()->getStudySlug() << " has not been loaded";
        }
        return study != nullptr;
      })
        .flat_map([server, unstored](std::shared_ptr<castor::Study> study) {return server->storeShortPseudonymInCastor(study, unstored); }) // Store the SP in Castor
        .map([pp = ctx->pp, column = unstored.getColumn().getFullName()](std::shared_ptr<castor::Participant> participant) {return ShortPseudonymEntry(pp, column, participant->getId()); }); // Produce return value
    }
    else {
#endif
      observable = server->generatePseudonym(
        unstored.getPrefix(),
        static_cast<int>(unstored.getLength())
      )
        .map([ctx, unstored](std::string sp) {return ShortPseudonymEntry(ctx->pp, unstored.getColumn().getFullName(), sp); });
#ifdef WITH_CASTOR
    }
#endif
    return observable.on_error_resume_next([first_error](std::exception_ptr error) { // Ignore Castor participant creation (or any other) failure
      if (!*first_error) {
        *first_error = error;
      }
      return rxcpp::observable<>::empty<ShortPseudonymEntry>();
    });
  })
    .map([server, ctx](const ShortPseudonymEntry& entry) { // Store each participant (ID) in shadow administration
    server->storeShortPseudonymShadow(ctx->encryptedIdentifier, entry.store.mColumn, entry.sp);
    return entry.store; // Return only the StoreData2Entry for further processing
  })
    .op(RxToVector()) // Collect all StoreData2Entry instances into a vector<>
    .flat_map([server, first_error](std::shared_ptr<std::vector<StoreData2Entry>> entries) { // Store SPs in storage facility, return observable<DataStorageResult2>
    return server->pClient->storeData2(*entries)
      .on_error_resume_next([first_error](std::exception_ptr ep) {
      if (!*first_error) {
        *first_error = ep;
      }
      LOG(LOG_TAG, error) << "Error while storing short pseudonyms: " << GetExceptionMessage(ep);
      return rxcpp::observable<>::empty<DataStorageResult2>();
    });
  })
    .reduce( // convert observable<DataStorageResult2> (possibly containing multiple entries) to observable<RegistrationResponse> with a single entry
      RegistrationResponse(),
      [](RegistrationResponse response, DataStorageResult2) {return response; }
    )
    .map([first_error](RegistrationResponse response) { // Serialize RegistrationResponse
    if (*first_error) {
      std::rethrow_exception(*first_error);
    }
    return rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(response))).as_dynamic();
  });
}

messaging::MessageBatches RegistrationServer::handleListCastorImportColumnsRequest(std::shared_ptr<ListCastorImportColumnsRequest> lpRequest) {
#ifndef WITH_CASTOR
  throw Error("Registration Server cannot retrieve Castor data because it wasn't compiled WITH_CASTOR");
#else
  std::optional<unsigned> answerSetCount;
  if (lpRequest->mAnswerSetCount != 0) {
    answerSetCount = lpRequest->mAnswerSetCount;
  }

  return getShortPseudonymDefinitions() // Get short pseudonym definitions
    .filter([lpRequest](const ShortPseudonymDefinition& sp) {return sp.getColumn().getFullName() == lpRequest->mSpColumn; }) // Find the SP definition matching the request
    .op(RxToVector()) // Aggregate to vector<> so we can check whether SP was found
    .map([lpRequest](std::shared_ptr<std::vector<ShortPseudonymDefinition>> sps) { // Produce matching SP or raise (network-portable) exception if not found
    if (sps->empty()) {
      throw Error("Short pseudonym " + lpRequest->mSpColumn + " not found");
    }
    return sps->front();
  })
    .flat_map([castor = getCastorConnection(), answerSetCount, client = pClient](const ShortPseudonymDefinition& sp) { // Get import column names for the SP
    return client->getAccessManagerProxy()->getColumnNameMappings()
      .flat_map([castor, sp, answerSetCount](ColumnNameMappings colMappings) {return castor::ImportColumnNamer(std::move(colMappings)).getImportableColumnNames(castor, sp, answerSetCount); });
  })
    .on_error_resume_next([](std::exception_ptr ep) -> rxcpp::observable<std::string> {throw Error(GetExceptionMessage(ep)); }) // Convert exceptions to network-portable Error instances
    .op(RxToVector()) // Aggregate column names into a vector<>
    .map([](std::shared_ptr<std::vector<std::string>> columnNames) { // Construct and serialize response
    ListCastorImportColumnsResponse response{ *columnNames };
    return rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(response))).as_dynamic();
  });
#endif
}


std::string RegistrationServer::describe() const {
  return "Registration Server";
}

}
