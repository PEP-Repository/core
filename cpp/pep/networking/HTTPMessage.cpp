#include <pep/networking/HTTPMessage.hpp>

#include <sstream>
#include <utility>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

namespace pep {

const std::string HTTPRequest::GET = "GET";
const std::string HTTPRequest::POST = "POST";

HTTPMessage::HTTPMessage(
  std::vector<std::shared_ptr<std::string>> bodyparts, 
  std::map<std::string, std::string, CaseInsensitiveCompare> headers)
  : headers(std::move(headers)), bodyparts(std::move(bodyparts)) {}

HTTPMessage::HTTPMessage(std::string body, std::map<std::string, std::string, CaseInsensitiveCompare> headers)
  : headers(std::move(headers)), bodyparts({std::make_shared<std::string>(std::move(body))}) {}

bool HTTPMessage::hasHeader(const std::string& name) const {
  return headers.find(name) != headers.end();
}

void HTTPMessage::setHeader(std::string name, std::string value) {
  headers[std::move(name)] = std::move(value);
}

std::string HTTPMessage::header(const std::string& name) const {
  return headers.at(name);
}

const std::map<std::string, std::string, CaseInsensitiveCompare>& 
  HTTPMessage::getHeaders() const
{
  return headers;
}

std::string HTTPMessage::getBody() const {
  std::string body;
  body.reserve(bodysize());
  for (const auto& bodypart : this->bodyparts) {
    body += *bodypart;
  }
  return body;
}

std::shared_ptr<std::string> HTTPMessage::getBodypart() const {
  // At the time of writing, a HTTPResponse always has at most one
  // body part.  This function asserts this, and returns the bodypart,
  // when it exists.
  switch (this->bodyparts.size()) {
  case 0:
    return std::make_shared<std::string>();
  case 1:
    return this->bodyparts[0];
  default:
    throw std::runtime_error("HTTPMessage::getBodypart: message "
      "has multiple bodyparts! ");
  }
}

size_t HTTPMessage::bodysize() const {
  size_t result = 0;
  for (const auto& bodypart : this->bodyparts) {
    result += bodypart->size();
  }
  return result;
}

std::map<std::string, std::string> HTTPMessage::getBodyAsFormData() const {
  if (auto it = headers.find("Content-Type"); it != headers.end()) {
    if (!boost::iequals(it->second, "application/x-www-form-urlencoded")) {
      throw std::runtime_error("Expected form Content-Type to be application/x-www-form-urlencoded");
    }
  }

  // https://url.spec.whatwg.org/#urlencoded-parsing
  const auto body = getBody();
  boost::urls::encoding_opts opts;
  opts.space_as_plus = true;
  boost::urls::params_view params(body, opts);
  std::map<std::string, std::string> ret;
  for (auto param : params) {
    if (!ret.try_emplace(std::move(param.key), std::move(param.value)).second) {
      throw std::runtime_error("double query or form parameters such as ?A=1&A=2 "
        "are not supported");
    }
  }
  (void) body; // body must remain valid while params is valid
  return ret;
}

HTTPResponse::HTTPResponse(const std::string& status, std::string body,
  std::map<std::string, std::string, CaseInsensitiveCompare> headers,
  bool completeHeaders
) : HTTPMessage(std::move(body), std::move(headers)) {

  std::istringstream statusStream(status);
  statusStream >> statuscode;
  size_t pos = static_cast<size_t>(statusStream.tellg()) + 1;
  if(pos < status.size())
    statusMessage = status.substr(pos);

  if(completeHeaders) {
    HTTPResponse::completeHeaders();
  }
}

HTTPResponse::HTTPResponse(
  unsigned int statuscode, std::string statusMessage, std::string body,
  std::map<std::string, std::string, CaseInsensitiveCompare> headers,
  bool completeHeaders
) : HTTPMessage(std::move(body), std::move(headers)),
  statuscode(statuscode),
  statusMessage(std::move(statusMessage)) {

  if(completeHeaders) {
    HTTPResponse::completeHeaders();
  }
}

std::string HTTPResponse::toString() const {
  std::ostringstream ss;
  ss << "HTTP/1.1 " << statuscode << " " << statusMessage << "\r\n";
  
  for(const auto& header : HTTPMessage::headers) {
    ss << header.first << ": " << header.second << "\r\n";
  }

  ss << "\r\n";
  ss << HTTPMessage::getBody();
  return ss.str();
}

void HTTPResponse::completeHeaders()  {
  if(!hasHeader("Content-Length"))
    setHeader("Content-Length", std::to_string(HTTPMessage::bodysize()));
}

HTTPRequest::HTTPRequest(
    std::string host,
    std::string method,
    boost::urls::url uri,
    std::string body,
    std::map<std::string, std::string,CaseInsensitiveCompare> headers,
    bool completeHeaders)
: HTTPRequest(
    std::move(host),
    std::move(method),
    std::move(uri),
    {std::make_shared<std::string>(std::move(body))},
    std::move(headers),
    completeHeaders) {}

HTTPRequest::HTTPRequest(
    std::string method,
    boost::urls::url uri,
    std::string body,
    std::map<std::string, std::string, CaseInsensitiveCompare> headers,
    bool completeHeaders)
  : HTTPRequest(
      uri.host_name(),
      std::move(method),
      uri, // Do not move: uri.host_name() is called above (and writing another overload isn't worth the effort)
      std::move(body),
      std::move(headers),
      completeHeaders) {}

HTTPRequest::HTTPRequest(
    std::string host,
    std::string method,
    boost::urls::url uri,
    std::vector<std::shared_ptr<std::string>> bodyparts,
    std::map<std::string, std::string, CaseInsensitiveCompare> headers,
    bool completeHeaders)
  : HTTPMessage(std::move(bodyparts), std::move(headers)),
    host(std::move(host)),
    method(std::move(method)),
    mUri(std::move(uri)) {

  assert(this->mUri.host_name().empty() || this->mUri.host_name() == this->host);
  if(completeHeaders) {
    HTTPRequest::completeHeaders();
  }
}

std::string HTTPRequest::headerToString() const {
  std::ostringstream ss;
  ss << method << " " << mUri.encoded_target();
 
  ss << " HTTP/1.1\r\n";

  for(const auto& header : HTTPMessage::headers) {
    ss << header.first << ": " << header.second << "\r\n";
  }

  ss << "\r\n";
  return ss.str();
}

std::string HTTPRequest::toString() const {
  return this->headerToString() + this->HTTPMessage::getBody();
}

void HTTPRequest::completeHeaders() {
  if(!hasHeader("Content-Length"))
    setHeader("Content-Length", std::to_string(HTTPMessage::bodysize()));
  if(!hasHeader("Host"))
    setHeader("Host", host);
}

}
