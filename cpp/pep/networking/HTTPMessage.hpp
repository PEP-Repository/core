#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

#include <pep/utils/Compare.hpp>

namespace pep {

//! Base class for HTTP Request and Response messages
class HTTPMessage {
protected:
  std::map<std::string, std::string, CaseInsensitiveCompare> headers;
  std::vector<std::shared_ptr<std::string>> bodyparts;

  /*!
    * \brief Construct a message
    *
    * \param body Body of the message
    * \param headers HTTP headers of the message
    */
  HTTPMessage(const std::string& body, const std::map<std::string, std::string, CaseInsensitiveCompare>& headers);

  HTTPMessage(std::vector<std::shared_ptr<std::string>> bodyparts, 
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers);

  //! Construct an empty message
  HTTPMessage() {}

  size_t bodysize() const;

public:
  std::string getBody() const; // WARNING: might be expensive

  // WARNING: getBodypart() throws an exception when the message
  // has more than one bodypart.
  std::shared_ptr<std::string> getBodypart() const;

  //! \return `true` if header with name `name` exists in message headers. `false` otherwise.
  bool hasHeader(const std::string& name) const;
  //! Set header with `name` to `value`. Overwrites if header with `name` exists. Adds a new header otherwise.
  void setHeader(const std::string& name, const std::string& value);
  //! \return Value of header with `name`.
  std::string header(const std::string& name) const;
  //! \return The headers - make a copy if necessary!
  const std::map<std::string, std::string, CaseInsensitiveCompare>& getHeaders() const;

  std::map<std::string, std::string> getBodyAsFormData() const;

  std::vector<std::shared_ptr<std::string>>& getBodyparts() {
    return this->bodyparts;
  }
};

//! A HTTP Response
class HTTPResponse : public HTTPMessage {
private:
  unsigned int statuscode;
  std::string statusMessage;

public:
  //! Construct an empty response 
  HTTPResponse() {}
  /*!
   * \brief construct a new response
   * 
   * \param status String containing the status code and message, e.g. "404 Not Found"
   * \param body Body of the Request
   * \param headers HTTP headers of the request
   * \param completeHeaders shall we set "Host" and "Content-Length" headers for you?
   */
  HTTPResponse(const std::string& status, const std::string& body = "",
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers = std::map<std::string, std::string, CaseInsensitiveCompare>(),
          bool completeHeaders=true);
  /*!
   * \brief construct a new response
   * 
   * \param statuscode The status code, e.g. 404
   * \param statusMessage The status message e.g. "Not Found"
   * \param body Body of the Request
   * \param headers HTTP headers of the request
   * \param completeHeaders shall we set "Host" and "Content-Length" headers for you?
   */
  HTTPResponse(unsigned int statuscode, const std::string& statusMessage, const std::string& body = "",
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers = std::map<std::string, std::string, CaseInsensitiveCompare>(),
          bool completeHeaders=true);

  //! \return Valid HTTP string representation of the response
  std::string toString() const;

  //! \return HTTP status code of the response
  unsigned int getStatusCode() const {
    return statuscode;
  }

  void setStatusCode(unsigned int statuscode) {
    this->statuscode = statuscode;
  }

  //! \return Status message of the response
  const std::string& getStatusMessage() const {
    return statusMessage;
  }

  void setStatusMessage(const std::string& statusMessage) {
    this->statusMessage = statusMessage;
  }

  void completeHeaders();
};

//! A HTTP Request
class HTTPRequest : public HTTPMessage {
public:
  //! A Uri. Can be absolute or relative. In the latter case protocol, hostname and port are unset.
  class Uri {
public:
    //! \brief Parse a string into a Uri
    //! \param uri The string to parse. If it has a recognized protocol (http or https), it will be parsed as absolute. Otherwise it will be relative
    //! \param queries Additional query-string parameters to set
    Uri(const std::string& uri, const std::map<std::string, std::string>& queries = std::map<std::string, std::string>());
    //! \brief Parse a string into a Uri
    //! \param uri The string to parse. If it has a recognized protocol (http or https), it will be parsed as absolute. Otherwise it will be relative
    //! \param queryString Additional query-string parameters to set
    Uri(const std::string& uri, const std::string& queryString);

    const std::optional<std::string>& getProtocol() const;
    const std::optional<std::string>& getHostname() const;
    const std::optional<std::uint16_t>& getPort() const;
    const std::string& getPath() const;
    std::string getQueryString() const;
    bool isRelative() const;

    //! \param relative Whether to add the protocol, hostname and port to the string represenation, or only the relative part
    std::string toString(bool relative = false) const;

    //! \return `true` if query with name `name` exists in querystring. `false` otherwise.
    bool hasQuery(const std::string& name) const;
    //! Set query with `name` to `value`. Overwrites if query with `name` exists. Adds a new query otherwise.
    void setQuery(const std::string& name, const std::string& value);
    //! \return Value of query with `name`.
    std::string query(const std::string& name) const;
    //! \return The queries - don't forget to make a copy if necessary!
    const std::map<std::string, std::string>& getQueries() const;

  private:
    std::optional<std::string>  protocol;
    std::optional<std::string> hostname;
    std::optional<std::uint16_t> port;
    std::string path;
    std::map<std::string, std::string> queries;

    void splitUriComponents(std::string_view uri);
  };

  static const std::string GET;
  static const std::string POST;

  /*!
    * \brief Construct a new Request
    *
    * \param host header to use for the Request
    * \param method HTTP Method of the Request
    * \param uri URI to request
    * \param body Body of the Request
    * \param headers HTTP headers of the request
    * \param completeHeaders shall we set "Host" and "Content-Length" headers for you?
    */
  HTTPRequest(const std::string& host,
          const std::string& method, const Uri& uri,
          const std::string& body = "",
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers = std::map<std::string, std::string, CaseInsensitiveCompare>(),
          bool completeHeaders=true);

  /*!
    * \brief Construct a new Request
    *
    * \param method HTTP Method of the Request
    * \param uri URI to request
    * \param body Body of the Request
    * \param headers HTTP headers of the request
    * \param completeHeaders shall we set "Host" and "Content-Length" headers for you?
    */
  HTTPRequest(
          const std::string& method, const Uri& uri,
          const std::string& body = "",
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers = std::map<std::string, std::string, CaseInsensitiveCompare>(),
          bool completeHeaders=true);

  /*!
    * \brief Construct a new Request
    *
    * \param host header to use for the Request
    * \param method HTTP Method of the Request
    * \param uri URI to request
    * \param bodyparts Body of the Request
    * \param headers HTTP headers of the request
    * \param completeHeaders shall we set "Host" and "Content-Length" headers for you?
    */
  HTTPRequest(const std::string& host,
          const std::string& method, const Uri& uri,
          std::vector<std::shared_ptr<std::string>> bodyparts,
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers = std::map<std::string, std::string, CaseInsensitiveCompare>(),
          bool completeHeaders=true);

  /*!
    * \brief Construct a new Request
    *
    * \param host header to use for the Request
    * \param method HTTP Method of the Request
    * \param uriString URI to request
    * \param body Body of the Request
    * \param headers HTTP headers of the request
    * \param queries The querystring parameters
    * \param completeHeaders shall we set "Host" and "Content-Length" headers for you?
    */
  HTTPRequest(const std::string& host,
          const std::string& method, const std::string& uriString,
          const std::string& body = "",
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers = std::map<std::string, std::string, CaseInsensitiveCompare>(),
          const std::map<std::string, std::string>& queries = std::map<std::string, std::string>(),
          bool completeHeaders=true);
  /*!
    * \brief Construct a new Request
    *
    * \param host header to use for the Request
    * \param method HTTP Method of the Request
    * \param uriString URI to request
    * \param body Body of the Request
    * \param headers HTTP headers of the request
    * \param queryString The queryString to be parsed into the queries field
    * \param completeHeaders shall we set "Host" and "Content-Length" headers for you?
    */
  HTTPRequest(const std::string& host,
          const std::string& method, const std::string& uriString,
          const std::string& body,
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers,
          const std::string& queryString,
          bool completeHeaders=true);

  //! \return Valid HTTP string representation of the request
  std::string toString() const;

  // HTTP string representation of only the header of the request.
  // If the body if the request is large, then it's faster to keep the body
  // and header separate.
  std::string headerToString() const;

  //! \return HTTP Method of the request
  const std::string& getMethod() const {
    return method;
  }

  const std::string& getHost() const {
    return host;
  }

  const Uri& getUri() const {
    return uri;
  }

  Uri& getUri() {
    return uri;
  }

  //! \return `true` if query with name `name` exists in querystring. `false` otherwise.
  bool hasQuery(const std::string& name) const;
  //! Set query with `name` to `value`. Overwrites if query with `name` exists. Adds a new query otherwise.
  void setQuery(const std::string& name, const std::string& value);
  //! \return Value of query with `name`.
  std::string query(const std::string& name) const;
  //! \return The queries - don't forget to make a copy if necessary!
  const std::map<std::string, std::string>& getQueries() const;

  void completeHeaders();

private:
  std::string host;
  std::string method;
  Uri uri;
};

/*!
 * \brief A callback function to be invoked when an HTTPRequest is (about to be) sent.
 */
typedef std::function<void(std::shared_ptr<const HTTPRequest>)> HttpRequestNotificationFunction;

}
