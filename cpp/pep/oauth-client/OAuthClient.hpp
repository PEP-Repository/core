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
  using GetAuthorizeUriFn = std::function<std::string (std::string redirectUri, std::optional<std::string_view>)>;
  using AuthorizationMethod = std::function<rxcpp::observable<AuthorizationResult> (
    std::shared_ptr<boost::asio::io_context> io_context,
    GetAuthorizeUriFn getAuthorizeUri)>;

  struct Parameters {
    /// The io_context to run on
    std::shared_ptr<boost::asio::io_context> io_context;
    /// The "OAuthServer" part of the client config
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
  boost::urls::url getAuthorizationUri(std::optional<std::string_view> state) const;
  rxcpp::observable<std::string> doTokenRequest(std::string_view code);

  std::shared_ptr<boost::asio::io_context> ioContext_;
  AuthorizationMethod authorizationMethod_;
  std::string requestUrl_;
  std::string tokenUrl_;
  std::string codeVerifier_;
  std::string redirectUrl_;
  //For local testing we use a self-signed HTTPS certificate for the authserver. We need to tell the HTTPS client to trust this certificate, so we need to know the path to the used certificate.
  //If this is left unset, the system CA store is used.
  std::optional<std::filesystem::path> caCertFilepath_;
  bool longLived_;
  std::optional<std::chrono::seconds> validityDuration_;


  friend class SharedConstructor<OAuthClient>;
};
}

