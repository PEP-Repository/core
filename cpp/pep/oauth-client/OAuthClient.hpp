#pragma once

#include <pep/networking/HTTPMessage.hpp>
#include <pep/utils/Configuration_fwd.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/utils/OperationResult.hpp>

#include <pep/async/IoContext_fwd.hpp>

#include <rxcpp/rx-lite.hpp>
#include <filesystem>

namespace pep {

using AuthorizationResult = OperationResult<std::string>;

class OAuthClient : public std::enable_shared_from_this<OAuthClient>, public SharedConstructor<OAuthClient>  {
public:
  using AuthorizationMethod = std::function<rxcpp::observable<AuthorizationResult> (
    std::shared_ptr<boost::asio::io_context> io_context,
    std::function<std::string (std::string redirectUri)> getAuthorizeUri)>;

  struct Parameters {
    /// The io_context to run on
    std::shared_ptr<boost::asio::io_context> io_context;
    /// The "AuthenticationServer" part of the client config
    const Configuration& config;
    /// Method to retrieve the authorization code, see e.g. BrowserAuthorization & ConsoleAuthorization
    AuthorizationMethod authorizationMethod;
    /// Whether a long-lived token should be requested
    bool longLived = false;
    /// If a long-lived token is requested, how long it should be valid. Use std::nullopt if the maximum allowed validity duration should be requested
    std::optional<std::chrono::seconds> validityDuration = std::nullopt;
  };

protected:
  OAuthClient(Parameters params);

public:
  rxcpp::observable<AuthorizationResult> run();

private:
  boost::urls::url getAuthorizationUri() const;
  rxcpp::observable<std::string> doTokenRequest(const std::string& code);

  std::shared_ptr<boost::asio::io_context> mIoContext;
  AuthorizationMethod mAuthorizationMethod;
  std::string mRequestUrl;
  std::string mTokenUrl;
  std::string mCodeVerifier;
  std::string mRedirectUrl;
  //For local testing we use a self-signed HTTPS certificate for the authserver. We need to tell the HTTPS client to trust this certificate, so we need to know the path to the used certificate.
  //If this is left unset, the system CA store is used.
  std::optional<std::filesystem::path> mCaCertFilepath;
  bool mLongLived;
  std::optional<std::chrono::seconds> mValidityDuration;


  friend class SharedConstructor<OAuthClient>;
};
}

