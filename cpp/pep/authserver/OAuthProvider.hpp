#pragma once

#include <cstdint>
#include <optional>
#include <unordered_set>

#include <boost/asio/io_service.hpp>
#include <rxcpp/rx-lite.hpp>

#include <pep/utils/Configuration.hpp>
#include <pep/httpserver/HTTPServer.hpp>
#include <pep/templating/TemplateEnvironment.hpp>


namespace pep {
class AuthserverBackend;

class OAuthProvider {
public:
  static const std::string RESPONSE_TYPE_CODE;
  static const std::string GRANT_TYPE_AUTHORIZATION_CODE;

// Prevent build failure due to ERROR_ACCESS_DENIED being defined (to a numeric value). See #1051
#pragma push_macro("ERROR_ACCESS_DENIED")
#undef ERROR_ACCESS_DENIED
  static const std::string ERROR_INVALID_REQUEST;
  static const std::string ERROR_INVALID_CLIENT;
  static const std::string ERROR_ACCESS_DENIED;
  static const std::string ERROR_UNAUTHORIZED_CLIENT;
  static const std::string ERROR_UNSUPPORTED_RESPONSE_TYPE;
  static const std::string ERROR_UNSUPPORTED_GRANT_TYPE;
  static const std::string ERROR_INVALID_SCOPE;
  static const std::string ERROR_SERVER_ERROR;
  static const std::string ERROR_TEMPORARILY_UNAVAILABLE;
  static const std::string ERROR_INVALID_GRANT;

#pragma pop_macro("ERROR_ACCESS_DENIED")

  class Parameters {
    public:
      Parameters(std::shared_ptr<boost::asio::io_service> io_service, const Configuration& config);

      unsigned short getHttpPort() const;
      std::chrono::seconds getActiveGrantExpiration() const;
      const std::string& getSpoofKey() const;
      const std::optional<std::filesystem::path>& getHttpsCertificateFile() const;
      std::shared_ptr<boost::asio::io_service> getIOservice() const;

      void check() const;

    private:
      uint16_t httpPort = 0;
      std::chrono::seconds activeGrantExpiration = std::chrono::seconds::zero();
      std::string spoofKey;
      //HTTPSClient does not support plain HTTP. On production environments, there is an apache2 server that handles HTTPS. But for local testing we need HTTPS, and therefore a certificate.
      //If this is left unset, plain HTTP is used
      std::optional<std::filesystem::path> httpsCertificateFile;
      std::shared_ptr<boost::asio::io_service> io_service;
  };

  OAuthProvider(const Parameters& params, std::shared_ptr<AuthserverBackend> authserverBackend);
  ~OAuthProvider();
private:
  struct grant {
    std::string clientId;
    int64_t internalId;
    std::string humanReadableId;
    std::string usergroup;
    std::string redirectUri;
    std::string codeChallenge;
    std::optional<std::chrono::seconds> validity; //std::nullopt if no long lived token is requested
    std::chrono::time_point<std::chrono::steady_clock> createdAt; //We don't care about the actual clock time, we only want to measure the time that has passed. Therefore: steady_clock.

    grant(const std::string& clientId, int64_t internalId, const std::string& humanReadableId, const std::string& usergroup, const std::string& redirectUri, const std::string& codeChallenge, std::optional<std::chrono::seconds> validity)
      : clientId(clientId), internalId(internalId), humanReadableId(humanReadableId), usergroup(usergroup), redirectUri(redirectUri), codeChallenge(codeChallenge), validity(validity), createdAt(std::chrono::steady_clock::now()) {}
  };

  void addActiveGrant(const std::string& code, grant g);
  std::optional<grant> getActiveGrant(const std::string& code);

  HTTPResponse handleAuthorizationRequest(HTTPRequest request);
  HTTPResponse handleTokenRequest(HTTPRequest request);
  HTTPResponse handleCodeRequest(HTTPRequest request);


  std::unordered_set<std::string> getRegisteredRedirectURIs(const std::string& clientId);

  std::shared_ptr<HTTPServer> httpServer;
  std::unordered_map<std::string, grant> activeGrants;
  rxcpp::composite_subscription activeGrantsCleanupSubscription;
  std::chrono::seconds activeGrantExpiration;
  std::string spoofKey;
  std::shared_ptr<AuthserverBackend> authserverBackend;
  std::shared_ptr<boost::asio::io_service> io_service;

  TemplateEnvironment mTemplates;

};
}
