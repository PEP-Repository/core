#pragma once

#include <pep/auth/OAuthToken.hpp>
#include <pep/keyserver/KeyServerMessages.hpp>
#include <pep/keyserver/tokenblocking/Blocklist.hpp>
#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/server/Server.hpp>

namespace pep {

class KeyServer : public Server {
public:
  class Parameters : public Server::Parameters {
  public:
    Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

    ServerTraits serverTraits() const noexcept override { return ServerTraits::KeyServer(); }

    /*!
     * \return The client CA private key
     */
    const AsymmetricKey& getClientCAPrivateKey() const;
    /*!
     * \param privateKey The client CA private key
     */
    void setClientCAPrivateKey(const AsymmetricKey& privateKey);

    /*!
     * \return The certificate chain corresponding with the client CA private key
     */
    const std::optional<X509CertificateChain>& getClientCACertificateChain();
    /*!
     * \param certificateChain The certificate chain corresponding with the client CA private key
     */
    void setClientCACertificateChain(const X509CertificateChain& certificateChain);

    /*!
     * \return The oauth token secret, shared with the authentication server
     */
    const std::string& getOauthTokenSecret() const;
    /*!
     * \param oauthTokenSecret The oauth token secret, shared with the authentication server
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
    void check() const override;

  private:
    AsymmetricKey mClientCAPrivateKey;
    std::optional<X509CertificateChain> mClientCACertificateChain;
    std::string mOauthTokenSecret;
    std::optional<std::filesystem::path> mBlocklistStoragePath;
  };

public:
  explicit KeyServer(std::shared_ptr<Parameters>);

private:
  messaging::MessageBatches handlePingRequest(std::shared_ptr<PingRequest> request);
  messaging::MessageBatches handleUserEnrollmentRequest(std::shared_ptr<EnrollmentRequest> enrollmentRequest);
  messaging::MessageBatches handleTokenBlockingListRequest(std::shared_ptr<SignedTokenBlockingListRequest> signedRequest);
  messaging::MessageBatches handleTokenBlockingCreateRequest(std::shared_ptr<SignedTokenBlockingCreateRequest> signedRequest);
  messaging::MessageBatches handleTokenBlockingRemoveRequest(std::shared_ptr<SignedTokenBlockingRemoveRequest> signedRequest);

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
