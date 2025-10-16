#pragma once
/* The "CastorConnection" class provides an interface to interact with the Castor EDC (server).
 * Its network connectivity is provided by the nested "Implementor" struct, which is
 * defined in the .cpp file and considered an implementation detail.
 * This allows the "CastorConnection" class to be used without be(com)ing dependent on our networking code.
 */

#include <pep/crypto/Timestamp.hpp>
#include <pep/networking/EndPoint.hpp>
#include <pep/networking/HTTPMessage.hpp>
#include <pep/utils/Event.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/castor/ApiKey.hpp>

#include <optional>
#include <filesystem>

#include <pep/async/IoContext_fwd.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include <rxcpp/rx-lite.hpp>

namespace pep {
namespace castor {

using JsonPtr = std::shared_ptr<const boost::property_tree::ptree>;

class Study;

//! The state the authentication is in
enum AuthenticationState { UNAUTHENTICATED, AUTHENTICATION_ERROR, AUTHENTICATING, AUTHENTICATED };

//! Describes the status of the authentication
struct AuthenticationStatus {
  //! The state the authentication is in.
  /*! If the token is expired, the state will remain AUTHENTICATED
   */
  AuthenticationState state;

  static const std::chrono::seconds EXPIRY_MARGIN;

  //! The authentication token
  std::string token;

  //! The calculated time of expiration of the token.
  /*! There is no margin used in the calculation. When checking the expiration, using a margin may be desirable
   */
  std::optional<Timestamp> expires;

  //! The exception, if an error occured
  std::exception_ptr exceptionPtr;

  /*!
   * \brief Construct an AUTHENTICATED AuthenticationStatus
   * \param token The received token
   * \param expiresIn The number of seconds until expiration of the token
   */
  AuthenticationStatus(const std::string& token, std::chrono::seconds expiresIn)
    : state(AUTHENTICATED), token(token), expires(Timestamp::now() + expiresIn) {}

  /*!
   * \brief Construct an AUTHENTICATION_ERROR AuthenticationStatus
   * \param exception_ptr The exception that occured causing the authentication error
   */
  AuthenticationStatus(const std::exception_ptr& exception_ptr)
    : state(AUTHENTICATION_ERROR), exceptionPtr(exception_ptr) {}

  /*!
   * \brief Construct an AuthenticationStatus with the given state
   * \param state The state of the AuthenticationStatus
   */
  AuthenticationStatus(const AuthenticationState& state = UNAUTHENTICATED) : state(state) {}

  /*!
   * \return true if the state is AUTHENTICATED, and the token has not expired, taking EXPIRY_MARGIN_SECONDS into account. false otherwise
   */
  bool authenticated() const;
};

/*!
 * \brief Thrown when the Castor API responds with an error
 *
 * This excludes things like network errors while using the Castor API, or server problems at Castor preventing the API from responding
 */
class CastorException : public std::runtime_error {
private:
  /*!
   * \brief Construct a CastorException from a HTTP status code, title and detail
   * \param status The HTTP status code returned by the API
   * \param title The title of the error
   * \param detail The description of the error
   */
  CastorException(const unsigned status, const std::string& title, const std::string& detail)
    : std::runtime_error("Castor returned status " + std::to_string(status) + " (" + title + "): " + detail), status(status), title(title), detail(detail) {}

public:
  static CastorException FromErrorResponse(const HTTPResponse& response, const std::string& additionalInfo);

  const unsigned status;
  const std::string title;
  const std::string detail;
};

class CastorConnection : public std::enable_shared_from_this<CastorConnection>, public SharedConstructor<CastorConnection> {
  friend class SharedConstructor<CastorConnection>;
private:
  struct Implementor;
  std::unique_ptr<Implementor> mImplementor;
  EventSubscription mOnRequestForwarding;

private:
  CastorConnection(const std::filesystem::path& apiKeyFile, std::shared_ptr<boost::asio::io_context> io_context);
  CastorConnection(const EndPoint& endPoint, const ApiKey& apiKey, std::shared_ptr<boost::asio::io_context> io_context, const std::optional<std::filesystem::path>& caCert = std::nullopt);

public:
  static const int RECORD_EXISTS = 422;
  static const int NOT_FOUND = 404;

  ~CastorConnection() noexcept;

  /*!
   * \brief Event that's notified when an HTTPRequest is (about to be) sent.
   */
  const Event<CastorConnection, std::shared_ptr<const HTTPRequest>> onRequest;

  // TODO: abstract entire interface away from HTTP
  std::shared_ptr<HTTPRequest> makeGet(const std::string& path);
  std::shared_ptr<HTTPRequest> makePost(const std::string& path, const std::string& body);
  rxcpp::observable<JsonPtr> sendCastorRequest(std::shared_ptr<HTTPRequest> request);

  rxcpp::observable<JsonPtr> getJsonEntries(const std::string& apiPath, const std::string& embeddedItemsNodeName);

  /*!
    * \brief Get the status of the authentication to castor
    * \return Observable that immediately emits the current AuthenticationStatus and will emit updates to the status
    */
  rxcpp::observable<AuthenticationStatus> authenticationStatus();

  //! Request a new authentication token from Castor
  void reauthenticate();

  /*!
   * \return Observable that, if no error occurs, emits a Study for all studies the authenticated user has access to
   */
  rxcpp::observable<std::shared_ptr<Study>> getStudies();
  /*!
   * \param slug The slug of the study to get. In the study settings in Castor this is called Study ID
   * \return Observable that, if no error occurs, emits a Study for the first study the authenticated user has access to, with the given slug.
   */
  rxcpp::observable<std::shared_ptr<Study>> getStudyBySlug(const std::string& slug);
};

}
}
