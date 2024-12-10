#pragma once

#include <pep/auth/OAuthToken.hpp>
#include <pep/keyserver/KeyServerMessages.hpp>
#include <pep/keyserver/tokenblocking/Blocklist.hpp>
#include <pep/server/Server.hpp>

namespace pep {

class KeyServer : public Server {
public:
  class Parameters : public Server::Parameters {
  public:
    Parameters(std::shared_ptr<boost::asio::io_service> io_service, const Configuration& config);

    /*!
     * \return The client CA private key
     */
    const AsymmetricKey& getClientCAPrivateKey() const;
    /*!
     * \param privateKey The client CA private key
     * \return The Parameters instance itself
     */
    void setClientCAPrivateKey(const AsymmetricKey& privateKey);

    /*!
     * \return The certificate chain corresponding with the client CA private key
     */
    X509CertificateChain& getClientCACertificateChain();
    /*!
     * \param certificateChain The certificate chain corresponding with the client CA private key
     * \return The Parameters instance itself
     */
    void setClientCACertificateChain(const X509CertificateChain& certificateChain);

    /*!
     * \return The oauth token secret, shared with the authentication server
     */
    const std::string& getOauthTokenSecret() const;
    /*!
     * \param The oauth token secret, shared with the authentication server
     * \return The Parameters instance itself
     */
    void setOauthTokenSecret(const std::string& oauthTokenSecret);

    /*!
     * \return The path where the blocklist of the keyserver is stored on disk
     */
    const std::optional<std::filesystem::path>& getBlocklistStoragePath() const;
    /*!
     * \param path The path where the blocklist of the keyserver is stored on disk
     */
    void setBlocklistStoragePath(const std::optional<std::filesystem::path>& path);

  protected:
    void check() override;

  private:
    AsymmetricKey mClientCAPrivateKey;
    X509CertificateChain mClientCACertificateChain;
    std::string mOauthTokenSecret;
    std::optional<std::filesystem::path> mBlocklistStoragePath;
  };

public:
  explicit KeyServer(std::shared_ptr<Parameters>);

protected:
  std::string describe() const override;

private:
  using SerializedResponse = rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>;
  SerializedResponse handleUserEnrollmentRequest(std::shared_ptr<EnrollmentRequest>);
  SerializedResponse handleTokenBlockingListRequest(std::shared_ptr<SignedTokenBlockingListRequest>);
  SerializedResponse handleTokenBlockingCreateRequest(std::shared_ptr<SignedTokenBlockingCreateRequest>);
  SerializedResponse handleTokenBlockingRemoveRequest(std::shared_ptr<SignedTokenBlockingRemoveRequest>);

private:
  /// Checks if the request is valid and throws an Error if it is not.
  void checkValid(const EnrollmentRequest&) const;

  X509Certificate generateCertificate(const X509CertificateSigningRequest&) const;

  bool isValid(const OAuthToken& authToken, const std::string& commonName, const std::string& organisationalUnit) const;

  AsymmetricKey mClientCAPrivateKey;
  X509CertificateChain mClientCACertificateChain;
  std::string mOauthTokenSecret;
  std::unique_ptr<tokenBlocking::Blocklist> mBlocklist;
};

} // namespace pep
