#pragma once

#include <pep/networking/HttpClient.hpp>
#include <pep/castor/CastorConnection.hpp>

namespace pep {
namespace castor {

//! Class to connect to the Castor API
class CastorClient : public std::enable_shared_from_this<CastorClient>, public SharedConstructor<CastorClient>, private LifeCycler {
  friend class SharedConstructor<CastorClient>;

public:
  ~CastorClient() noexcept override { this->shutdown(); } // TODO: prevent exceptions from escaping

  void start();
  void shutdown();

  //! Request a new authentication token from Castor
  void reauthenticate();

  /*!
   * \brief Make a GET Request
   *
   * \param path Path to the resource to get
   * \param useBasePath Whether \p path should be relative to the \ref setBasePath "base path" or not
   * \return The created Request
   */
  std::shared_ptr<HTTPRequest> makeGet(const std::string& path, const bool& useBasePath = true);

  /*!
   * \brief Make a POST Request
   *
   * \param path Path to the resource to post
   * \param body Body of the Request
   * \param useBasePath Whether \p path should be relative to the \ref setBasePath "base path" or not
   * \return The created Request
   */
  std::shared_ptr<HTTPRequest> makePost(const std::string& path,
    const std::string& body,
    const bool& useBasePath = true);

  const Event<CastorClient, std::shared_ptr<const HTTPRequest>> onRequest;

  /*!
   * \brief Send a request to the Castor API and parse the response as %Json
   *
   * Authorization header will always be added.
   *
   * \param request The request to send
   * \return Observable that, if no error occurs, emits a JsonPtr, or multiple in case of a paged response
   */
  rxcpp::observable<JsonPtr> sendCastorRequest(std::shared_ptr<HTTPRequest> request);

  /*!
   * \brief Get the status of the authentication to castor
   * \return Observable that immediately emits the current AuthenticationStatus and will emit updates to the status
   */
  rxcpp::observable<AuthenticationStatus> authenticationStatus() {
    return authenticationSubject.get_observable();
  };

private:
  rxcpp::observable<HTTPResponse> sendRequest(std::shared_ptr<HTTPRequest> request);
  rxcpp::observable<HTTPResponse> sendPreAuthorizedRequest(std::shared_ptr<HTTPRequest> request);
  rxcpp::observable<JsonPtr> handleCastorResponse(std::shared_ptr<HTTPRequest> request, const HTTPResponse& response);

  rxcpp::subjects::behavior<AuthenticationStatus>
    authenticationSubject{ AuthenticationStatus(UNAUTHENTICATED) };
  static constexpr int PAGE_SIZE = 1000;
  static const std::string BASE_PATH;

private:
  CastorClient(boost::asio::io_context& ioContext, const EndPoint& endPoint, std::string clientId, std::string clientSecret, std::optional<std::filesystem::path> caCertFilepath = std::nullopt);

  std::shared_ptr<networking::HttpClient> mHttp;
  EventSubscription mOnRequestForwarding;
  std::string mClientId;
  std::string mClientSecret;
};

}
}
