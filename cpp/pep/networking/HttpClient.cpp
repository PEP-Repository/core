#include <pep/networking/HttpClient.hpp>
#include <pep/networking/HTTPMessage.hpp>
#include <pep/networking/Tcp.hpp>
#include <pep/networking/Tls.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/async/RxTimeout.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Timestamp.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>

#include <chrono>

using namespace std::ranges;

namespace pep::networking {

namespace {

#define PEP_CRLF "\r\n"

const std::string LogTag = "HTTP client";

struct ProtocolProperties {
  bool tls;
  std::string scheme;
  uint16_t defaultPort;
};

const std::vector<ProtocolProperties> SupportedProtocols = {
  { false, "http", 80 },
  { true, "https", 443}
};

void TrimOutsideWhitespace(std::string& str) {
  boost::trim_if(str, boost::is_space());
}

std::string FormatHttpUrl(bool tls, const EndPoint& endPoint) {
  auto protocol = find(SupportedProtocols, tls, &ProtocolProperties::tls);
  assert(protocol != SupportedProtocols.end());

  auto result = protocol->scheme + "://" + endPoint.hostname;
  if (endPoint.port != protocol->defaultPort) {
    result += ":" + std::to_string(endPoint.port);
  }

  return result;
}

using RetryDelay = HttpClient::RetryParameters::Delay;

/// \remark Clients shouldn't automatically retry non-idempotent requests unless they know the original request wasn't applied,
///         see https://www.rfc-editor.org/rfc/rfc9110#section-9.2.2 . Of our methods, only POST isn't idempotent.
bool IsRetryableError(HttpMethod method, unsigned statusCode) {
  switch (statusCode) {
  case 408: // Request Timeout: server "did not receive a complete request message" (RFC 9110 section 15.5.9)
  case 429: // Too Many Requests: rate limiting (RFC 6585 section 4), i.e. the server refused to handle the request
  case 503: // Service Unavailable: server "is currently unable to handle the request" (RFC 9110 section 15.6.4)
    return true; // The server (presumably) didn't apply the request, so resending is harmless
  case 500: // Internal Server Error
  case 502: // Bad Gateway
  case 504: // Gateway Timeout
    return method != HttpMethod::Post; // The server (or upstream) may have (partially) applied the request, so only resend idempotent requests
  default:
    return false;
  }
}

/// \brief Parses the value of a "Retry-After" header, which is either a number of seconds or an HTTP-date.
/// \return The delay, or std::nullopt if the value couldn't be parsed.
/// \remark See https://www.rfc-editor.org/rfc/rfc9110#field.retry-after
std::optional<RetryDelay> ParseRetryAfter(std::string_view value) {
  if (std::chrono::seconds::rep seconds{};
    boost::conversion::try_lexical_convert(value, seconds)) {
    return std::chrono::seconds{seconds};
  }

  // Parsing as a number didn't work, try to parse as timestamp:
  try {
    // Round up, since the server wants us to wait at least until the specified time
    return std::chrono::ceil<RetryDelay>(TimestampFromHttpDate(value) - TimeNow());
  }
  catch (const std::exception&) {
    return std::nullopt;
  }
}

boost::urls::url UrlPlusRelative(const boost::urls::url& url, const std::string& relative) {
  if (url.has_query()) {
    throw std::runtime_error("Can't append a relative URL onto a base URL that has queries");
  }

  std::string base = url.buffer();
  if (!base.ends_with('/') && !relative.starts_with('/')) {
    base += '/';
  }
  return boost::urls::url(base + relative);
}

}

HttpClient::Parameters::Parameters(boost::asio::io_context& ioContext, boost::urls::url absoluteBase, std::optional<std::string> expectedCommonName)
  : ioContext_(ioContext), tls_(false), baseUri_(std::move(absoluteBase)) {
  this->validateBaseUri();

  auto protocol = find(SupportedProtocols, baseUri_.scheme(), &ProtocolProperties::scheme);
  if (protocol == SupportedProtocols.end()) {
    throw std::runtime_error("Unsupported protocol " + std::string(baseUri_.scheme()));
  }

  tls_ = protocol->tls;

  endPoint_.hostname = baseUri_.host();
  endPoint_.port = baseUri_.port_number();
  if (endPoint_.port == 0) {
    endPoint_.port = protocol->defaultPort;
  }
  if (expectedCommonName.has_value()) {
    endPoint_.expectedCommonName = *expectedCommonName;
  }
}

HttpClient::Parameters::Parameters(boost::asio::io_context& ioContext, bool tls, const EndPoint& endPoint, const std::optional<std::string>& relativeBase)
  : ioContext_(ioContext), tls_(tls), baseUri_(FormatHttpUrl(tls, endPoint)), endPoint_(endPoint) {
  this->validateBaseUri();
  if (relativeBase.has_value()) {
    baseUri_ = UrlPlusRelative(baseUri_, *relativeBase);
    this->validateBaseUri();
  }
}

std::shared_ptr<Client> HttpClient::Parameters::createBinaryClient() const {
  std::unique_ptr<Protocol::ClientParameters> parameters;
  if (tls_) {
    auto tls = std::make_unique<Tls::ClientParameters>(ioContext_, endPoint_);
    tls->caCertFilePath(this->caCertFilepath());
    parameters = std::move(tls);
  }
  else {
    parameters = std::make_unique<Tcp::ClientParameters>(ioContext_, endPoint_);
  }
  return Client::Create(*parameters, this->reconnectParameters());
}

void HttpClient::Parameters::validateBaseUri() const {
  if (!baseUri_.has_scheme() || !baseUri_.has_authority()) {
    throw std::runtime_error("HttpClient requires an absolute base URI");
  }
  if (baseUri_.has_query()) {
    throw std::runtime_error("HttpClient base URI may not contain queries"); // You can add those to the HTTPRequest that you get from HttpClient::makeRequest
  }
  if (baseUri_.host().empty()) {
    throw std::runtime_error("HttpClient base URI requires a host name");
  }
}

HttpClient::HttpClient(Parameters parameters)
  : parameters_(std::move(parameters)) {
}

void HttpClient::shutdown() {
  if (this->status() != Status::Uninitialized && this->status() < Status::Finalizing) {
    this->setStatus(Status::Finalizing);
    this->stop();
  }
  this->setStatus(Status::Finalized);
}

bool HttpClient::isRunning() const noexcept {
  auto status = this->status();
  return status > Status::Uninitialized && status < Status::Finalizing;
}

void HttpClient::start() {
  auto status = this->status();
  if (status > Status::Initialized) {
    throw std::runtime_error("Can't (re)start an HttpClient after it has been shut down");
  }
  if (binaryClient_ != nullptr) {
    throw std::runtime_error("Can't start an HttpClient more than once");
  }
  this->setStatus(Status::Initializing);

  binaryClient_ = parameters_.createBinaryClient();
  binaryClientConnectionAttempt_ = binaryClient_->onConnectionAttempt.subscribe([weak = WeakFrom(*this)](const networking::Connection::Attempt::Result& result) {
    auto self = weak.lock();
    if (result && self != nullptr) {
      self->connection_ = *result; // TODO: clear when connection loses connectivity (and always check whether connection_ != nullptr before using it)
      self->setStatus(Status::Initialized);
      self->ensureSend();
    }
    });
  binaryClient_->start();
}

HTTPRequest HttpClient::makeRequest(HttpMethod method, const std::optional<std::string>& path) const {
  const auto& baseUri = parameters_.baseUri();
  return HTTPRequest(baseUri.host(), method, UrlPlusRelative(baseUri, path.value_or(std::string())), std::vector<std::shared_ptr<std::string>>(), std::map<std::string, std::string, CaseInsensitiveCompare>(), false);
}

std::string HttpClient::pathFromUrl(const boost::urls::url& full) {
  std::string str = full.c_str();
  std::string base = parameters_.baseUri().c_str();
  if (!str.starts_with(base)) {
    throw std::runtime_error("Client for " + base + " can't extract path from unrelated URL " + str);
  }
  return str.substr(base.size());
}

rxcpp::observable<HTTPResponse> HttpClient::sendRequest(HTTPRequest request) {
  PEP_LOG(LogTag, Severity::Debug) << "Sending HTTP request for " << request.uri();
  if (!this->isRunning()) {
    throw std::runtime_error("HttpClient must be running to send a request");
  }
  if (!std::string(request.uri().c_str()).starts_with(parameters_.baseUri().c_str())) {
    throw std::runtime_error("Can't send request that doesn't match the HTTP client's base URI");
  }

  request.completeHeaders();
  return this->sendAttempt(MakeSharedCopy(std::move(request)), 0U);
}

rxcpp::observable<HTTPResponse> HttpClient::sendAttempt(std::shared_ptr<const HTTPRequest> request, unsigned retries) {
  // Give each attempt its own instance so that "unpend" can't confuse it with an earlier attempt
  auto sendable = MakeSharedCopy(*request);
  onRequest.notify(sendable);

  return CreateObservable<HTTPResponse>([self = SharedFrom(*this), sendable](rxcpp::subscriber<HTTPResponse> subscriber) {
    self->pendingRequests_.push(MakeSharedCopy(PendingRequest{ sendable, subscriber }));

    // Stop (re)sending if/when the subscriber unsubscribes
    subscriber.add([self, sendable]() { self->unpend(sendable); });

    self->ensureSend();
    }).subscribe_on(ObserveOnAsio(parameters_.ioContext()))
    .concat_map([self = SharedFrom(*this), request, retries](const HTTPResponse& response) -> rxcpp::observable<HTTPResponse> {
      auto delay = self->getRetryDelay(*request, response, retries);
      if (!delay.has_value()) {
        return rxcpp::observable<>::just(response);
      }

      PEP_LOG(LogTag, Severity::Warning) << "Received HTTP status " << response.getStatusCode() << " for " << request->uri()
        << "; resending in " << *delay << " (retry " << (retries + 1U) << " of " << self->parameters_.retryParameters().maxRetries << ')';
      auto& ioContext = self->parameters_.ioContext();
      return RxAsioTimer(*delay, ioContext, ObserveOnAsio(ioContext))
        .concat_map([self, request, retries, response](const FakeVoid&) -> rxcpp::observable<HTTPResponse> {
          if (!self->isRunning()) { // We were shut down while waiting
            return rxcpp::observable<>::just(response);
          }
          return self->sendAttempt(request, retries + 1U);
          });
    });
}

std::optional<std::chrono::milliseconds> HttpClient::getRetryDelay(const HTTPRequest& request, const HTTPResponse& response, unsigned retries) const {
  const auto& parameters = parameters_.retryParameters();
  if (retries >= parameters.maxRetries || !IsRetryableError(request.getMethod(), response.getStatusCode())) {
    return std::nullopt;
  }

  std::optional<RetryDelay> requested;
  if (parameters.delayCallback) {
    requested = parameters.delayCallback(response);
  }
  if (!requested.has_value() && response.hasHeader("Retry-After")) {
    auto header = response.header("Retry-After");
    requested = ParseRetryAfter(header);
    if (!requested.has_value()) {
      PEP_LOG(LogTag, Severity::Warning) << "Ignoring unparseable Retry-After header value: " << header;
    }
  }

  if (!requested.has_value()) {
    return std::chrono::ceil<std::chrono::milliseconds>(parameters.backoff.timeoutAfter(retries));
  }
  if (*requested > parameters.maxDelay) {
    PEP_LOG(LogTag, Severity::Warning) << "Not resending HTTP request for " << request.uri() << " (status " << response.getStatusCode()
      << "): requested delay of " << *requested << " exceeds maximum of " << parameters.maxDelay;
    return std::nullopt;
  }
  return std::max(*requested, RetryDelay::zero());
}

void HttpClient::stop() {
  if (binaryClient_ != nullptr) {
    binaryClientConnectionAttempt_.cancel();
    auto subscription = std::make_shared<EventSubscription>();
    if (this->status() == Status::Finalizing) {
      // Notify that we've been finalized when the binary client becomes finalized
      *subscription = binaryClient_->onStatusChange.subscribe([subscription, weak = WeakFrom(*this)](StatusChange change) {
        assert(change.updated >= Status::Finalizing);
        auto self = weak.lock();
        if (self != nullptr && change.updated == Status::Finalized) {
          assert(self->status() == Status::Finalizing);
          self->setStatus(Status::Finalized);
        }
        });
    }
    binaryClient_->shutdown();
    binaryClient_ = nullptr;
  }
}

void HttpClient::restart() {
  this->setStatus(Status::Initializing);
  this->stop();
  this->start();
}

bool HttpClient::continueSending(std::exception_ptr error) {
  if (error != nullptr) {
    PEP_LOG(LogTag, Severity::Debug) << "Error: " << GetExceptionMessage(error);

    // Reconnect to prevent the binary transport from remaining in a possibly invalid state
    // TODO: only do this if the binary transport didn't close (or reset) itself already
    this->restart();
    this->finishSending(error);
    return false;
  }

  if (pendingRequests_.empty() || pendingRequests_.front() != sending_) {
    this->finishSending();
    return false;
  }

  return true;
}

void HttpClient::ensureSend() {
  auto status = this->status();
  assert(status == Status::Finalized || binaryClient_ != nullptr);
  if (sending_ != nullptr || status >= Status::Finalizing || connection_ == nullptr || !connection_->isConnected()) {
    return;
  }

  // Don't send abandoned requests
  while (!pendingRequests_.empty() && !pendingRequests_.front()->subscriber.is_subscribed()) {
    pendingRequests_.pop();
  }
  if (pendingRequests_.empty()) {
    return;
  }

  sending_ = pendingRequests_.front();
  response_ = HTTPResponse();

  auto header = MakeSharedCopy(sending_->request->headerToString());
  connection_->asyncWrite(header->data(), header->size(), [self = SharedFrom(*this), header](const SizedTransfer::Result& result) {
    self->handleRequestPartWritten(result, 0);
    });
}

bool HttpClient::unpend(std::shared_ptr<HTTPRequest> request) {
  if (!pendingRequests_.empty() && pendingRequests_.front()->request == request) {
    pendingRequests_.pop();
    return true;
  }

  return false;
}

void HttpClient::finishSending(std::exception_ptr error) {
  if (error == nullptr) {
    assert(sending_ != nullptr);
    auto subscriber = sending_->subscriber;
    if (this->unpend(sending_->request)) { // Otherwise the subscriber has already unsubscribed
      subscriber.on_next(response_);
      subscriber.on_completed();
    }
  }
  // TODO: else notify subscriber of the error (they may wish to abort instead of having us retry)

  sending_.reset();
  this->ensureSend();
}

void HttpClient::handleRequestPartWritten(const SizedTransfer::Result& result, size_t sentBodyParts) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  const auto& parts = sending_->request->getBodyparts();
  for (auto i = sentBodyParts; i < parts.size(); ++i) {
    const auto& part = parts[i];
    if (!part->empty()) {
      connection_->asyncWrite(part->data(), part->size(), [self = SharedFrom(*this), sent = i + 1](const SizedTransfer::Result& result) {
        self->handleRequestPartWritten(result, sent);
        });
      return;
    }
  }

  // Done sending body parts: start receiving the HTTPResponse
  connection_->asyncReadUntil(PEP_CRLF, [self = SharedFrom(*this)](const DelimitedTransfer::Result& result) {
    self->handleReadStatusLine(result);
    });
}

void HttpClient::handleReadStatusLine(const DelimitedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(result->ends_with(PEP_CRLF));
  std::istringstream responseStream(*result);

  std::string http_version;
  responseStream >> http_version;
  if (!http_version.starts_with("HTTP/")) {
    this->finishSending(std::make_exception_ptr(std::runtime_error("Invalid HTTP response: didn't start with required magic bytes")));
    return;
  }

  unsigned int statuscode{};
  responseStream >> statuscode;
  response_.setStatusCode(statuscode);

  std::string statusMessage;
  std::getline(responseStream, statusMessage);
  if (!responseStream) {
    this->finishSending(std::make_exception_ptr(std::runtime_error("Invalid HTTP response: status line unreadable")));
    return;
  }

  TrimOutsideWhitespace(statusMessage);
  response_.setStatusMessage(statusMessage);

  this->readHeaderLine();
}

void HttpClient::readHeaderLine() {
  connection_->asyncReadUntil(PEP_CRLF, [self = SharedFrom(*this)](const DelimitedTransfer::Result& result) {
    self->handleReadHeaderLine(result);
    });
}

void HttpClient::handleReadHeaderLine(const DelimitedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(result->ends_with(PEP_CRLF));
  auto length = result->size() - 2U;
  if (length == 0U) { // This is the empty line that precedes the body
    this->readBody();
  }
  else { // This is an actual "key: value" header line
    auto header = result->substr(0U, length);
    size_t index = header.find(':');
    if (index != std::string::npos) {
      std::string headerName = header.substr(0, index);
      std::string headerValue = header.substr(index + 1);
      TrimOutsideWhitespace(headerValue);
      response_.setHeader(headerName, headerValue);
    }
    else {
      PEP_LOG(LogTag, Severity::Warning) << "Ignoring malformed header: " << header << '\n';
    }
    this->readHeaderLine(); // Done processing this header line: read the next one
  }
}

void HttpClient::readBody() {
  auto transferEncodingHeader = response_.getHeaders().find("Transfer-Encoding");
  if (transferEncodingHeader != response_.getHeaders().end()) {
    if (!transferEncodingHeader->second.contains("chunked")) {
      // Since binaryClient_ may have received (or may still receive) stuff that we can't process, we can't (reliably) keep using it
      this->restart();
      this->finishSending(std::make_exception_ptr(std::runtime_error("Unsupported transfer encoding " + transferEncodingHeader->second)));
      return;
    }
    this->readChunkSize();
  }
  else {
    auto contentLengthHeader = response_.getHeaders().find("Content-Length");
    if (contentLengthHeader != response_.getHeaders().end()) {
      auto contentlength = boost::lexical_cast<size_t>(contentLengthHeader->second);
      if (contentlength > 0) {
        contentBuffer_.resize(contentlength);
        connection_->asyncRead(contentBuffer_.data(), contentBuffer_.size(), [self = SharedFrom(*this)](const SizedTransfer::Result& result) {
          self->handleReadKnownSizeBody(result);
          });
      }
      else {
        this->finishSending();
      }
    }
    else {
      connection_->asyncReadAll([self = SharedFrom(*this)](const DelimitedTransfer::Result& result) {
        self->handleReadConnectionBoundBody(result);
        });
    }
  }
}

void HttpClient::readChunkSize() {
  connection_->asyncReadUntil(PEP_CRLF, [self = SharedFrom(*this)](const DelimitedTransfer::Result& result) {
    self->handleReadChunkSize(result);
    });
}

void HttpClient::handleReadChunkSize(const DelimitedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(boost::ends_with(*result, PEP_CRLF));
  std::istringstream responseStream(result->substr(0, result->size() - 2));
  size_t chunkSize{};
  responseStream >> std::hex >> chunkSize;

  if (chunkSize > 0) {
    // Read the chunk, including the trailing PEP_CRLF
    contentBuffer_.resize(chunkSize + 2);
    connection_->asyncRead(contentBuffer_.data(), contentBuffer_.size(), [self = SharedFrom(*this)](const SizedTransfer::Result& result) {
      self->handleReadChunk(result);
      });
  }
  else {
    // We're processing the last (empty) chunk: read the PEP_CRLF that's written after its (empty) content
    contentBuffer_.resize(2);
    connection_->asyncRead(contentBuffer_.data(), contentBuffer_.size(), [self = SharedFrom(*this)](const SizedTransfer::Result& result) {
      if (!self->continueSending(result.exception())) {
        return;
      }

      assert(self->contentBuffer_ == PEP_CRLF);
      self->finishSending();
      });
  }
}

void HttpClient::handleReadChunk(const SizedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(boost::ends_with(contentBuffer_, PEP_CRLF));
  contentBuffer_.resize(contentBuffer_.size() - 2U);
  response_.getBodyparts().push_back(MakeSharedCopy(contentBuffer_));

  this->readChunkSize();
}

void HttpClient::handleReadKnownSizeBody(const SizedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(*result == contentBuffer_.size());
  this->handleReadBody(std::move(contentBuffer_));
}

void HttpClient::handleReadConnectionBoundBody(const DelimitedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }
  this->handleReadBody(*result);
}

void HttpClient::handleReadBody(std::string body) {
  response_.getBodyparts().push_back(MakeSharedCopy(std::move(body)));
  this->finishSending();
}

}
