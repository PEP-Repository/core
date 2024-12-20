#include <pep/networking/HTTPMessage.hpp>
#include <pep/utils/UriEncode.hpp>

#include <sstream>
#include <utility>
#include <boost/lexical_cast.hpp>
#include <unordered_map>

namespace pep {

const std::string HTTPRequest::GET = "GET";
const std::string HTTPRequest::POST = "POST";

const std::unordered_map<std::string, std::uint16_t> DEFAULT_PROTOCOL_PORTS = {// For now we only support recognizing http or https protocols
    {"http", 80},
    {"https", 443}
  };

static void parseUriEncodedParams(std::string_view inputString, std::map<std::string, std::string>& out) {
  while(inputString.length()>0) {
    size_t found = inputString.find('&');

    std::string_view params = inputString.substr(0, found);
    inputString = (found==std::string::npos ? "" : inputString.substr(found+1));

    found = params.find('=');
    std::string name = UriDecode(std::string(params.substr(0, found)), true);
    std::string value = UriDecode(std::string((found==std::string::npos ? "" : params.substr(found+1))), true);

    if (!out.try_emplace(name, value).second)
      throw std::runtime_error("double query or form parameters such as ?A=1&A=2 "
          "are currently not supported");
  }
}

HTTPMessage::HTTPMessage(
        std::vector<std::shared_ptr<std::string>> bodyparts, 
        const std::map<std::string, std::string, CaseInsensitiveCompare>& headers)
  : headers(headers), bodyparts(std::move(bodyparts)) {}

HTTPMessage::HTTPMessage(const std::string& body, const std::map<std::string, std::string, CaseInsensitiveCompare>& headers)
  : headers(headers), bodyparts({std::make_shared<std::string>(body)}) {}

bool HTTPMessage::hasHeader(const std::string& name) const {
  return headers.find(name) != headers.end();
}

void HTTPMessage::setHeader(const std::string& name, const std::string& value) {
  headers[name] = value;
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
  std::map<std::string, std::string> ret;
  parseUriEncodedParams(getBody(), ret);
  return ret;
}

HTTPResponse::HTTPResponse(const std::string& status, const std::string& body,
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers,
          bool completeHeaders) : HTTPMessage(body, headers) {
  std::istringstream statusStream(status);
  statusStream >> statuscode;
  size_t pos = static_cast<size_t>(statusStream.tellg()) + 1;
  if(pos < status.size())
    statusMessage = status.substr(pos);

  if(completeHeaders) {
    HTTPResponse::completeHeaders();
  }
}

HTTPResponse::HTTPResponse(unsigned int statuscode, const std::string& statusMessage, const std::string& body,
          const std::map<std::string, std::string, CaseInsensitiveCompare>& headers,
          bool completeHeaders) : HTTPMessage(body, headers), statuscode(statuscode), statusMessage(statusMessage) {

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

HTTPRequest::Uri::Uri(const std::string& uri, const std::map<std::string, std::string>& queries)
  : queries(queries) {
  splitUriComponents(uri);
}

HTTPRequest::Uri::Uri(const std::string& uri, const std::string& queryString) : Uri(uri, std::map<std::string, std::string>()) {
  parseUriEncodedParams(queryString, queries);
}

void HTTPRequest::Uri::splitUriComponents(std::string_view uri) {

  size_t questionPos = uri.find('?');

  if (questionPos != std::string::npos) {
    const std::string_view qs = uri.substr(questionPos+1); // ditch "?"
    uri = uri.substr(0, questionPos);

    parseUriEncodedParams(qs, queries);
  }

  for (auto& [prot, portNo] : DEFAULT_PROTOCOL_PORTS) {
    if (uri.starts_with(prot + "://")) {
      protocol = prot;
      port = portNo;
      uri = uri.substr(prot.length() + std::char_traits<char>::length("://"));
      break;
    }
  }

  if(protocol) {
    size_t slashPos = uri.find('/');
    if(slashPos != std::string::npos) {
      path = uri.substr(slashPos);
    }
    else {
      path = "/";
    }
    uri = uri.substr(0, slashPos);
    size_t colonPos = uri.find(':');
    if(colonPos != std::string::npos) {
      port = boost::lexical_cast<std::uint16_t>(uri.substr(colonPos + 1));
    }
    hostname = uri.substr(0, colonPos);
  }
  else {
    path = uri;
  }
}

const std::optional<std::string>& HTTPRequest::Uri::getProtocol() const {
  return protocol;
}

const std::optional<std::string>& HTTPRequest::Uri::getHostname() const {
  return hostname;
}

const std::optional<std::uint16_t>& HTTPRequest::Uri::getPort() const {
  return port;
}

const std::string& HTTPRequest::Uri::getPath() const {
  return path;
}

std::string HTTPRequest::Uri::getQueryString() const {
  std::ostringstream oss;
  if (!queries.empty()) {
    bool first = true;
    for (auto const& [name, value] : queries) {
      if(!first)
        oss << "&";
      first = false;
      oss << UriEncode(name) << "=" << UriEncode(value);
    }
  }
  return oss.str();
}

bool HTTPRequest::Uri::isRelative() const {
  assert(protocol.has_value() == hostname.has_value());
  return !protocol.has_value();
}

std::string HTTPRequest::Uri::toString(bool relative) const {
  std::ostringstream oss;
  if(!relative) {
    if(protocol) {
      assert(hostname && port); //Either we have a relative Uri, and protocol, hostname and port are all unset, or we have an absolute Uri, and they are all set.
      oss << *protocol << "://" << *hostname;
      assert(DEFAULT_PROTOCOL_PORTS.find(*protocol) != DEFAULT_PROTOCOL_PORTS.end());
      if(DEFAULT_PROTOCOL_PORTS.at(*protocol) != *port) {
        oss << ":" << *port;
      }
      if(path.length() > 0 && path[0] != '/') {
        oss << "/";
      }
    }
  }
  oss << path;
  if(!queries.empty()) {
    oss << "?";
    oss << getQueryString();
  }
  return oss.str();
}

bool HTTPRequest::Uri::hasQuery(const std::string& name) const {
  return queries.find(name) != queries.end();
}

void HTTPRequest::Uri::setQuery(const std::string& name, 
    const std::string& value) {
  queries[name] = value;
}

std::string HTTPRequest::Uri::query(const std::string& name) const {
  return queries.at(name);
}

const std::map<std::string, std::string>& 
  HTTPRequest::Uri::getQueries() const
{
  return queries;
}

HTTPRequest::HTTPRequest(const std::string& host,
                              const std::string& method,
                              const Uri& uri,
                              const std::string& body,
                              const std::map<std::string, std::string,CaseInsensitiveCompare>& headers,
                              bool completeHeaders)
  : HTTPMessage(body, headers), host(host), method(method), uri(uri) {

  assert(!uri.getHostname() || uri.getHostname() == host);
  if(completeHeaders) {
    HTTPRequest::completeHeaders();
  }
}

HTTPRequest::HTTPRequest(const std::string& method,
                              const Uri& uri,
                              const std::string& body,
                              const std::map<std::string, std::string, CaseInsensitiveCompare>& headers,
                              bool completeHeaders)
  : HTTPRequest(*uri.getHostname(), method, uri, body, headers, completeHeaders) {}

HTTPRequest::HTTPRequest(
        const std::string& host,
        const std::string& method,
        const Uri& uri,
        std::vector<std::shared_ptr<std::string>> bodyparts,
        const std::map<std::string, std::string, CaseInsensitiveCompare>& headers,
        bool completeHeaders)
  : HTTPMessage(std::move(bodyparts), headers), host(host), method(method), uri(uri) {

  assert(!uri.getHostname() || uri.getHostname() == host);
  if(completeHeaders) {
    HTTPRequest::completeHeaders();
  }
}

HTTPRequest::HTTPRequest(const std::string& host,
                              const std::string& method,
                              const std::string& uriString,
                              const std::string& body,
                              const std::map<std::string, std::string, CaseInsensitiveCompare>& headers,
                              const std::map<std::string, std::string>& queries,
                              bool completeHeaders)
  : HTTPRequest(host, method, Uri(uriString, queries), body, headers, completeHeaders)  {}

HTTPRequest::HTTPRequest(const std::string& host,
                              const std::string& method,
                              const std::string& uriString,
                              const std::string& body,
                              const std::map<std::string, std::string, CaseInsensitiveCompare>& headers,
                              const std::string& queryString,
                              bool completeHeaders)
  : HTTPRequest(host, method, Uri(uriString, queryString), body, headers, completeHeaders)  {}

std::string HTTPRequest::headerToString() const {
  std::ostringstream ss;
  ss << method << " " << uri.toString( true);
 
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

bool HTTPRequest::hasQuery(const std::string& name) const {
  return uri.hasQuery(name);
}

void HTTPRequest::setQuery(const std::string& name, const std::string& value) {
  uri.setQuery(name, value);
}

std::string HTTPRequest::query(const std::string& name) const {
  return uri.query(name);
}

const std::map<std::string, std::string>& HTTPRequest::getQueries() const {
  return uri.getQueries();
}

void HTTPRequest::completeHeaders() {
  if(!hasHeader("Content-Length"))
    setHeader("Content-Length", std::to_string(HTTPMessage::bodysize()));
  if(!hasHeader("Host"))
    setHeader("Host", host);
}

}
