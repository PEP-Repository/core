#pragma once

#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/async/RxCache.hpp>
#include <pep/server/SigningServer.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/registrationserver/RegistrationServerMessages.hpp>

#ifdef WITH_CASTOR
#include <pep/castor/Study.hpp>
#endif

#include <sqlite3.h>

namespace pep {
/*
 * The registration server provides functionality to register participants. It
 * generates short pseudonyms based on the provided configuration file and
 * stores these in PEP. It maintains a local list of all generated short
 * pseudonyms, without any additional information, in order to prevent
 * collisions in the short pseudonyms. A shadow registration is maintained
 * containing an encrypted identifier (provided by the client) and the tag and
 * short pseudonym (encrypted by the registration server). This is meant to
 * serve as a secure backup in case the short pseudonyms can no longer be
 * retrieved from PEP.
 */

class RegistrationServer : public SigningServer {
 public:
  class Parameters : public SigningServer::Parameters {
   public:
    Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

    /*!
     * \return PEP client used to store data
     */
    std::shared_ptr<CoreClient> getClient() const;
    /*!
     * \param client PEP client used to store data
     */
    void setClient(const std::shared_ptr<CoreClient> client);

    /*!
     * \return Path to the shadow storage file
     */
    const std::filesystem::path& getShadowStorageFile() const;
    /*!
     * \param shadowStorageFile Path to the shadow storage file
     */
    void setShadowStorageFile(const std::filesystem::path& shadowStorageFile);

    /*!
     * \return Public key of the shadow storage
     */
    const AsymmetricKey& getShadowPublicKey() const;
    /*!
     * \param shadowPublicKey Public key of the shadow storage
     */
    void setShadowPublicKey(const AsymmetricKey& shadowPublicKey);

#ifdef WITH_CASTOR
    /*!
     * \return The Castor client connection
     */
    std::shared_ptr<castor::CastorConnection> getCastorConnection() const;
    /*!
     * \param castorConnection The Castor client connection
     */
    void setCastorConnection(std::shared_ptr <castor::CastorConnection> castorConnection);
#endif

   protected:
    void check() const override;

   private:
    std::shared_ptr<CoreClient> client;
    std::filesystem::path shadowStorageFile;
    AsymmetricKey shadowPublicKey;
#ifdef WITH_CASTOR
    std::shared_ptr<castor::CastorConnection> castorConnection;
#endif
  };

public:
  explicit RegistrationServer(std::shared_ptr<Parameters> parameters);
  ~RegistrationServer() override;

protected:
  std::string describe() const override;
  std::vector<std::string> getChecksumChainNames() const override;
  void computeChecksumChainChecksum(
    const std::string& chain, std::optional<uint64_t> maxCheckpoint,
    uint64_t& checksum, uint64_t& checkpoint) override;

private:
  messaging::MessageBatches handleSignedPEPIdRegistrationRequest(std::shared_ptr<SignedPEPIdRegistrationRequest> lpRequest);
  messaging::MessageBatches handleSignedRegistrationRequest(std::shared_ptr<SignedRegistrationRequest> lpRequest);
  messaging::MessageBatches handleListCastorImportColumnsRequest(std::shared_ptr<ListCastorImportColumnsRequest> lpRequest);

private:
  class ShortPseudonymCache;

  ::sqlite3* pShadowStorage = nullptr;
  std::shared_ptr<CoreClient> pClient;
  AsymmetricKey shadowPublicKey;
  std::shared_ptr<RxCache<std::shared_ptr<GlobalConfiguration>>> mGlobalConfiguration;
  std::shared_ptr<ShortPseudonymCache> mShortPseudonyms;
#ifdef WITH_CASTOR
  std::shared_ptr<castor::CastorConnection> mCastorConnection;
  std::shared_ptr<RxCache<std::shared_ptr<castor::Study>>> mCastorStudies;

  std::shared_ptr<castor::CastorConnection> getCastorConnection() const;
  rxcpp::observable<std::shared_ptr<castor::Participant>> storeShortPseudonymInCastor(std::shared_ptr<castor::Study> study, ShortPseudonymDefinition definition);
#endif
  rxcpp::observable<std::string> generatePseudonym(std::string prefix, int len);

  rxcpp::observable<ShortPseudonymDefinition> getShortPseudonymDefinitions() const;

  /*!
   * \brief Opens the shadow storage DB.
   *
   * \return TRUE if the database was created, FALSE if not.
   */
  bool openDatabase(const std::filesystem::path& file);

  void closeDatabase() noexcept;

  /*!
   * \brief Gets all (participant and short) pseudonyms from Storage Facility, and initializes shadow storage if it doesn't exist.
   *
   * \return An observable emitting all (participant and short) pseudonyms known by Storage Facility.
   */
  rxcpp::observable<std::string> initPseudonymStorage(const std::filesystem::path& shadowStorageFile);

  size_t countShadowStoredEntries() const;

  /*!
   * \brief Store the tag and short pseudonym encrypted in the shadow SQLite database together with the encrypted identifier. It returns the SQLite return code for the query.
   *
   * \param encryptedIdentifier The encrypted identifier provided by the client
   * \param tag The tag of short pseudonym to be stored
   * \param shortPseudonym The short pseudonym to be stored
   */
  void storeShortPseudonymShadow(const std::string& encryptedIdentifier, const std::string& tag, const std::string& shortPseudonym);
};

}
