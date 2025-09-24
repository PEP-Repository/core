#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

#include <pep/utils/Compare.hpp>
#include <pep/networking/HttpMethod.hpp>

#include <boost/url/url.hpp>

namespace pep {

//! Base class for HTTP Request and Response messages
class HTTPMessage {
public:
  using HeaderMap = std::map<std::string, std::string, CaseInsensitiveCompare>;

protected:
  HeaderMap headers;
  std::vector<std::shared_ptr<std::string>> bodyparts;

  /*!
    * \brief Construct a message
    *
    * \param body Body of the message
    * \param headers HTTP headers of the message
    */
  HTTPMessage(std::string body, HeaderMap headers);

  HTTPMessage(
    std::vector<std::shared_ptr<std::string>> bodyparts,
    HeaderMap headers);

  //! Construct an empty message
  HTTPMessage() = default;

  size_t bodysize() const;

public:
  std::string getBody() const; // WARNING: might be expensive

  // WARNING: getBodypart() throws an exception when the message
  // has more than one bodypart.
  std::shared_ptr<std::string> getBodypart() const;

  //! \return `true` if header with name `name` exists in message headers. `false` otherwise.
  bool hasHeader(const std::string& name) const;
  //! Set header with `name` to `value`. Overwrites if header with `name` exists. Adds a new header otherwise.
  void setHeader(std::string name, std::string value);
  //! \return Value of header with `name`.
  std::string header(const std::string& name) const;
  //! \return The headers - make a copy if necessary!
  const HeaderMap& getHeaders() const;

  std::map<std::string, std::string> getBodyAsFormData() const;

  std::vector<std::shared_ptr<std::string>>& getBodyparts() {
    return this->bodyparts;
  }
};

//! A HTTP Response
class HTTPResponse : public HTTPMessage {
private:
  unsigned int statuscode{};
  std::string statusMessage;

public:
  //! Construct an empty response 
  HTTPResponse() = default;
  /*!
   * \brief construct a new response
   * 
   * \param status String containing the status code and message, e.g. "404 Not Found"
   * \param body Body of the Request
   * \param headers HTTP headers of the request
   * \param completeHeaders shall we set "Host" and "Content-Length" headers for you?
   */
  HTTPResponse(const std::string& status, std::string body = "",
          HeaderMap headers = {},
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
  HTTPResponse(unsigned int statuscode, std::string statusMessage, std::string body = "",
          HeaderMap headers = {},
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

  void setStatusMessage(std::string statusMessage) {
    this->statusMessage = std::move(statusMessage);
  }

  void completeHeaders();
};

//! A HTTP Request
class HTTPRequest : public HTTPMessage {
public:
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
  HTTPRequest(
    std::string host,
    networking::HttpMethod method,
    boost::urls::url uri,
    std::string body = "",
    HeaderMap headers = {},
    bool completeHeaders = true);

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
    networking::HttpMethod method,
    boost::urls::url uri,
    std::string body = "",
    HeaderMap headers = {},
    bool completeHeaders = true);

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
  HTTPRequest(
    std::string host,
    networking::HttpMethod method,
    boost::urls::url uri,
    std::vector<std::shared_ptr<std::string>> bodyparts,
    HeaderMap headers = {},
    bool completeHeaders = true);

  //! \return Valid HTTP string representation of the request
  std::string toString() const;

  // HTTP string representation of only the header of the request.
  // If the body if the request is large, then it's faster to keep the body
  // and header separate.
  std::string headerToString() const;

  //! \return HTTP Method of the request
  const networking::HttpMethod& getMethod() const {
    return mMethod;
  }

  const std::string& getHost() const {
    return mHost;
  }

  const boost::urls::url& uri() const {
    return mUri;
  }

  boost::urls::url& uri() {
    return mUri;
  }

  void completeHeaders();

private:
  void ensureHeader(const std::string& key, const std::string& value);

  std::string mHost;
  networking::HttpMethod mMethod;
  boost::urls::url mUri;
};

}
