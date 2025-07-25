#pragma once

#include <pep/networking/HTTPSClient.hpp>
#include <pep/castor/CastorConnection.hpp>

namespace pep {
namespace castor {

//! Class to connect to the Castor API
class CastorClient : public HTTPSClient {
 public:
  //! Parameters for the CastorClient
  class Parameters : public HTTPSClient::Parameters {
   public:
    /*!
     * \param clientID OAuth Client ID of the castor account to use with the API
     */
    void setClientID(const std::string& clientID) {
      this->clientID = clientID;
    }

    //! \return OAuth Client ID of the castor account to use with the API
    std::string getClientID() {
      return clientID;
    }

    /*!
     * \param clientSecret OAuth Client Secretof the castor account to use with the API
     */
    void setClientSecret(const std::string& clientSecret) {
      this->clientSecret = clientSecret;
    }

    //! \return OAuth Client Secretof the castor account to use with the API
    std::string getClientSecret() {
      return clientSecret;
    }

   protected:
    void check() const override {
      if (clientID.empty())
        throw std::runtime_error("clientID must be set");
      if (clientSecret.empty())
        throw std::runtime_error("clientSecret must be set");
      HTTPSClient::Parameters::check();
    }

   private:
    std::string clientID = "";
    std::string clientSecret = "";
  };

  class Connection : public HTTPSClient::Connection {
  public:
    Connection(std::shared_ptr<CastorClient> client)
      : HTTPSClient::Connection(client) {
      setBasePath("/api/");
    }

    //! Request a new authentication token from Castor
    void reauthenticate();

    /*!
     * \brief Send a request, with an authorization header added to it
     *
     * \param request The request to send
     * \return Observable that, if no error occurs, emits exactly one HTTPResponse
     */
    rxcpp::observable<HTTPResponse> sendRequest(std::shared_ptr<HTTPRequest> request) override;

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
    }

  protected:
    std::string describe() const override {
      return "Castor";
    }

    std::shared_ptr<CastorClient> getClient() const noexcept { return std::static_pointer_cast<CastorClient>(HTTPSClient::Connection::getClient()); }

  private:
    rxcpp::observable<HTTPResponse> sendPreAuthorizedRequest(std::shared_ptr<HTTPRequest> request);
    rxcpp::observable<JsonPtr> handleCastorResponse(std::shared_ptr<HTTPRequest> request, const HTTPResponse& response);

    rxcpp::subjects::behavior<AuthenticationStatus>
      authenticationSubject{ AuthenticationStatus(UNAUTHENTICATED) };
    int pageSize = 1000;
  };

  CastorClient(std::shared_ptr<Parameters> parameters) : HTTPSClient(parameters), mClientId(parameters->getClientID()), mClientSecret(parameters->getClientSecret()) {}

private:
  std::string mClientId;
  std::string mClientSecret;
};

}
}
