#pragma once

#include <pep/httpserver/HTTPServer.hpp>

#include <pep/utils/Configuration.hpp>
#include <pep/utils/Shared.hpp>

#include <rxcpp/rx-lite.hpp>

namespace pep {

class OAuthClient : public std::enable_shared_from_this<OAuthClient>, public SharedConstructor<OAuthClient>  {
public:
  struct Parameters {
    /// The io_service to run on
    const std::shared_ptr<boost::asio::io_service>& ioService;
    /// The "AuthenticationServer" part of the client config
    const Configuration& config;
    /// Whether we are running in a limited environment, i.e. we can't open a browser directly, and can't easily listen on localhost for a redirect from the browser. This is e.g. the case when running on a server, or via Docker.
    bool limitedEnvironment = false;
    /// Whether a long-lived token should be requested
    bool longLived = false;
    /// If a long-lived token is requested, how long it should be valid. Use std::nullopt if the maximum allowed validity duration should be requested
    std::optional<std::chrono::seconds> validityDuration = std::nullopt;
  };

  class OAuthError : public std::exception  {
  private:
    std::string mError;
    std::string mDescription;
    std::string mWhat;

  public:
    OAuthError(std::string error, std::string description);

    const char* what() const noexcept override;
  };
protected:
  OAuthClient(Parameters params);

public:
  rxcpp::observable<std::string> run();

private:
  void doTokenRequest(const std::string& code, const rxcpp::subscriber<std::string>& subscriber);

  std::shared_ptr<boost::asio::io_service> mIoService;
  std::shared_ptr<HTTPServer> mHttpServer;
  std::string mRequestUrl;
  std::string mTokenUrl;
  std::string mCodeVerifier;
  //For local testing we use a self-signed HTTPS certificate for the authserver. We need to tell the HTTPS client to trust this certificate, so we need to know the path to the used certificate.
  //If this is left unset, the system CA store is used.
  std::optional<std::filesystem::path> mCaCertFilepath;
  bool mLimitedEnvironment;
  bool mLongLived;
  std::optional<std::chrono::seconds> mValidityDuration;


  friend class SharedConstructor<OAuthClient>;
};
}

