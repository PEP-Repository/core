#pragma once

#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/utils/Shared.hpp>

#include <cstdint>
#include <optional>
#include <unordered_set>

#include <pep/async/IoContext_fwd.hpp>
#include <rxcpp/rx-lite.hpp>

#include <pep/utils/Configuration_fwd.hpp>
#include <pep/httpserver/HTTPServer.hpp>
#include <pep/templating/TemplateEnvironment.hpp>

namespace pep {
class AuthserverBackend;

class OAuthProvider : public std::enable_shared_from_this<OAuthProvider>, public SharedConstructor<OAuthProvider> {
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
      Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config);

      uint16_t getHttpPort() const;
      std::chrono::seconds getActiveGrantExpiration() const;
      const std::string& getSpoofKey() const;
      const std::optional<std::filesystem::path>& getHttpsCertificateFile() const;
      std::shared_ptr<boost::asio::io_context> getIoContext() const;

      void check() const;

    private:
      uint16_t httpPort = 0;
      std::chrono::seconds activeGrantExpiration = std::chrono::seconds::zero();
      std::string spoofKey;
      //On production environments, there is an apache2 server that handles HTTPS. But for local testing we want HTTPS, and therefore a certificate.
      //If this is left unset, plain HTTP is used
      //TODO: determine if we indeed want/need HTTPS for local testing, or whether we can use plain HTTP instead (HttpClient supports it)
      std::optional<std::filesystem::path> httpsCertificateFile;
      std::shared_ptr<boost::asio::io_context> io_context;
  };

  ~OAuthProvider();
private:
  OAuthProvider(const Parameters& params, std::shared_ptr<AuthserverBackend> authserverBackend);

  struct grant {
    std::string clientId;
    std::string humanReadableId;
    UserGroup usergroup;
    std::string redirectUri;
    std::string codeChallenge;
    std::optional<std::chrono::seconds> validity; //std::nullopt if no long lived token is requested
    std::chrono::time_point<std::chrono::steady_clock> createdAt; //We don't care about the actual clock time, we only want to measure the time that has passed. Therefore: steady_clock.

    grant(const std::string& clientId, const std::string& humanReadableId, UserGroup usergroup, const std::string& redirectUri, const std::string& codeChallenge, std::optional<std::chrono::seconds> validity)
      : clientId(clientId), humanReadableId(humanReadableId), usergroup(std::move(usergroup)), redirectUri(redirectUri), codeChallenge(codeChallenge), validity(validity), createdAt(std::chrono::steady_clock::now()) {}
  };

  void addActiveGrant(const std::string& code, grant g);
  std::optional<grant> getActiveGrant(const std::string& code);

  rxcpp::observable<HTTPResponse> handleAuthorizationRequest(HTTPRequest request, std::string remoteIp);
  HTTPResponse handleTokenRequest(HTTPRequest request, std::string remoteIp);
  HTTPResponse handleCodeRequest(HTTPRequest request, std::string remoteIp);


  std::unordered_set<std::string> getRegisteredRedirectURIs(const std::string& clientId);

  std::shared_ptr<HTTPServer> httpServer;
  std::unordered_map<std::string, grant> activeGrants;
  rxcpp::composite_subscription activeGrantsCleanupSubscription;
  std::chrono::seconds activeGrantExpiration;
  std::string spoofKey;
  std::shared_ptr<AuthserverBackend> authserverBackend;
  std::shared_ptr<boost::asio::io_context> io_context;

  TemplateEnvironment mTemplates;
  friend class SharedConstructor<OAuthProvider>;
};
}
