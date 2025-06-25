#include <pep/application/Application.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/utils/File.hpp>

#include <sqlite3.h>

#include <iostream>
#include <fstream>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

using namespace pep;

namespace {

const std::string LOG_TAG ("DumpShadowAdministration");

class DumpShadowAdministrationApplication : public pep::Application {
  static void DumpShadowAdministration(const char* filename, const AsymmetricKey shadowPrivateKey) {
    ::sqlite3* pDB;
    int err;

    // Open SQLite database
    err = sqlite3_open(filename, &pDB);
    if (err != SQLITE_OK) {
      LOG(LOG_TAG, warning) << "Error opening SQLite database: " << err;
      throw std::runtime_error("Error opening SQLite database");
    }

    sqlite3_stmt* pStmt;

    err = sqlite3_prepare(pDB, "SELECT * FROM ShadowShortPseudonyms", -1, &pStmt, nullptr);
    if (err != SQLITE_OK) {
      LOG(LOG_TAG, warning) << "Error opening SQLite database: " << sqlite3_errmsg(pDB);
      throw std::runtime_error("Error opening SQLite database");
    }

    size_t len;
    std::string identifier, shortPseudonym;

    err = sqlite3_step(pStmt);
    while (err == SQLITE_ROW) {
      len = static_cast<size_t>(sqlite3_column_bytes(pStmt, 0));
      identifier = shadowPrivateKey.decrypt(std::string(static_cast<const char*>(sqlite3_column_blob(pStmt, 0)), len));

      len = static_cast<size_t>(sqlite3_column_bytes(pStmt, 1));
      shortPseudonym = shadowPrivateKey.decrypt(std::string(static_cast<const char*>(sqlite3_column_blob(pStmt, 1)), len));

      std::cout << identifier << ";" << shortPseudonym << std::endl;

      err = sqlite3_step(pStmt);
    }

    sqlite3_close(pDB);
  }

  static void StoreShortPseudonymShadow(::sqlite3* pDB, const AsymmetricKey& shadowPublicKey, const std::string& identifier, const std::string& shortPseudonym) {
    sqlite3_stmt* insertStmt;

    std::string encryptedIdentifier = shadowPublicKey.encrypt(identifier);
    std::string encryptedShortPseudonym = shadowPublicKey.encrypt(shortPseudonym);

    if(sqlite3_prepare_v2(pDB, "INSERT INTO ShadowShortPseudonyms(EncryptedIdentifier, EncryptedShortPseudonym) VALUES(?, ?)", -1, &insertStmt, nullptr) != SQLITE_OK) {
      LOG(LOG_TAG, warning) << "Error occured: " << sqlite3_errmsg(pDB);
      throw std::runtime_error("Error occured");
    }

    if (encryptedIdentifier.size() > static_cast<unsigned>(INT_MAX)) {
      LOG(LOG_TAG, error) << "Encrypted identifier too large to store";
      throw std::runtime_error("Encrypted identifier too large to store");
    }
    sqlite3_bind_blob(insertStmt, 1, encryptedIdentifier.data(), static_cast<int>(encryptedIdentifier.size()), SQLITE_STATIC);
    if (encryptedShortPseudonym.size() > static_cast<unsigned>(INT_MAX)) {
      LOG(LOG_TAG, error) << "Encrypted short pseudonym too large to store";
      throw std::runtime_error("Encrypted short pseudonym too large to store");
    }
    sqlite3_bind_blob(insertStmt, 2, encryptedShortPseudonym.data(), static_cast<int>(encryptedShortPseudonym.size()), SQLITE_STATIC);

    sqlite3_step(insertStmt);

    if (sqlite3_finalize(insertStmt) != SQLITE_OK) {
      LOG(LOG_TAG, warning) << "Error occured while storing in shadow administration: " << sqlite3_errmsg(pDB);
      throw std::runtime_error("Error occured while storing in shadow administration");
    }
  }

  static void CreateShadowAdministration(const char* filenameDB, const AsymmetricKey& shadowPublicKey, const char* filenameInput) {
    ::sqlite3* pDB;
    int err;

    // Open SQLite database for shadow administration of identifiers/short pseudonyms
    err = sqlite3_open(filenameDB, &pDB);
    if(err != SQLITE_OK) {
      LOG(LOG_TAG, warning) << "Error opening SQLite database: " << sqlite3_errmsg(pDB);
      throw std::runtime_error("Error opening SQLite database");
    }

    // Create table if it does not exist yet
    err = sqlite3_exec(pDB, "CREATE TABLE IF NOT EXISTS `ShadowShortPseudonyms` (`EncryptedIdentifier`  BLOB, `EncryptedShortPseudonym`  BLOB, `Id` INTEGER PRIMARY KEY AUTOINCREMENT);", nullptr, nullptr, nullptr);
    if(err != SQLITE_OK) {
      LOG(LOG_TAG, warning) << "Error creating SQLite table: " << sqlite3_errmsg(pDB);
      throw std::runtime_error("Error creating SQLite table");
    }

    // Insert all pseudonyms
    std::ifstream input(filenameInput);

    std::string line;
    std::vector<std::string> splitLine;
    while(std::getline(input, line)) {
      boost::split(splitLine, line, [](char ch){return ch == ';';});
      StoreShortPseudonymShadow(pDB, shadowPublicKey, splitLine[0], splitLine[1]);
    }

    input.close();

    sqlite3_close(pDB);
  }

  std::string getDescription() const override {
    return "Process shadow database for encrypted identifiers and short pseudonyms";
  }

  class DumpCommand : public commandline::ChildCommandOf<DumpShadowAdministrationApplication> {
  protected:
    commandline::Parameters getSupportedParameters() const override {
      return commandline::ChildCommandOf<DumpShadowAdministrationApplication>::getSupportedParameters()
        + commandline::Parameter("private-key", "Path to private key file for the database").value(commandline::Value<std::filesystem::path>().positional().required())
        + commandline::Parameter("database", "Path to shadow database file").value(commandline::Value<std::filesystem::path>().positional().required());
    }

    int execute() override {
      const auto& values = this->getParameterValues();
      auto shadowPrivateKey = pep::ReadFile(values.get<std::filesystem::path>("private-key"));
      auto database = values.get<std::filesystem::path>("database");
      DumpShadowAdministration(database.string().c_str(), AsymmetricKey(shadowPrivateKey));
      return EXIT_SUCCESS;
    }

  public:
    explicit DumpCommand(DumpShadowAdministrationApplication& parent)
      : commandline::ChildCommandOf<DumpShadowAdministrationApplication>("dump", "Dump shadow administration contents", parent) {
    }
  };

  class CreateCommand : public commandline::ChildCommandOf<DumpShadowAdministrationApplication> {
  protected:
    commandline::Parameters getSupportedParameters() const override {
      return commandline::ChildCommandOf<DumpShadowAdministrationApplication>::getSupportedParameters()
        + commandline::Parameter("public-key", "Path to public key file for the database").value(commandline::Value<std::filesystem::path>().positional().required())
        + commandline::Parameter("database", "Path to shadow database file to create").value(commandline::Value<std::filesystem::path>().positional().required())
        + commandline::Parameter("input-file", "Path to input file").value(commandline::Value<std::filesystem::path>().positional().required());
    }

    int execute() override {
      const auto& values = this->getParameterValues();
      auto shadowPublicKey = pep::ReadFile(values.get<std::filesystem::path>("public-key"));
      auto database = values.get<std::filesystem::path>("database");
      auto inputFile = values.get<std::filesystem::path>("input-file");
      CreateShadowAdministration(database.string().c_str(), AsymmetricKey(shadowPublicKey), inputFile.string().c_str());
      return EXIT_SUCCESS;
    }

  public:
    explicit CreateCommand(DumpShadowAdministrationApplication& parent)
      : commandline::ChildCommandOf<DumpShadowAdministrationApplication>("create", "Create a new shadow administration file", parent) {
    }
  };

  std::vector<std::shared_ptr<commandline::Command>> createChildCommands() override {
    return { std::make_shared<DumpCommand>(*this), std::make_shared<CreateCommand>(*this) };
  }
};

}

PEP_DEFINE_MAIN_FUNCTION(DumpShadowAdministrationApplication)
