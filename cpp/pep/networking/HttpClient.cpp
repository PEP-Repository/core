#include <pep/networking/HttpClient.hpp>
#include <pep/networking/HTTPMessage.hpp>
#include <pep/networking/Tcp.hpp>
#include <pep/networking/Tls.hpp>
#include <pep/async/CreateObservable.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>

namespace pep::networking {

namespace {

#define CRLF "\r\n"

const std::string LOG_TAG = "HTTP client";

struct ProtocolProperties {
  bool tls;
  std::string scheme;
  uint16_t defaultPort;
};

const std::vector<ProtocolProperties> SUPPORTED_PROTOCOLS = {
  { false, "http", 80 },
  { true, "https", 443}
};

void TrimOutsideWhitespace(std::string& str) {
  boost::trim_if(str, boost::is_any_of(" \r\n\t"));
}

std::string FormatHttpUrl(bool tls, const EndPoint& endPoint) {
  auto protocol = std::find_if(SUPPORTED_PROTOCOLS.begin(), SUPPORTED_PROTOCOLS.end(), [tls](const ProtocolProperties& candidate) { return candidate.tls == tls; });
  assert(protocol != SUPPORTED_PROTOCOLS.end());

  auto result = protocol->scheme + "://" + endPoint.hostname;
  if (endPoint.port != protocol->defaultPort) {
    result += ":" + std::to_string(endPoint.port);
  }

  return result;
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
  : mIoContext(ioContext), mTls(false), mBaseUri(std::move(absoluteBase)) {
  this->validateBaseUri();

  auto protocol = std::find_if(SUPPORTED_PROTOCOLS.begin(), SUPPORTED_PROTOCOLS.end(), [scheme = mBaseUri.scheme()](const ProtocolProperties& candidate) { return candidate.scheme == scheme; });
  if (protocol == SUPPORTED_PROTOCOLS.end()) {
    throw std::runtime_error("Unsupported protocol " + std::string(mBaseUri.scheme()));
  }

  mTls = protocol->tls;

  mEndPoint.hostname = mBaseUri.host();
  mEndPoint.port = mBaseUri.port_number();
  if (mEndPoint.port == 0) {
    mEndPoint.port = protocol->defaultPort;
  }
  if (expectedCommonName.has_value()) {
    mEndPoint.expectedCommonName = *expectedCommonName;
  }
}

HttpClient::Parameters::Parameters(boost::asio::io_context& ioContext, bool tls, const EndPoint& endPoint, const std::optional<std::string>& relativeBase)
  : mIoContext(ioContext), mTls(tls), mBaseUri(FormatHttpUrl(tls, endPoint)), mEndPoint(endPoint) {
  this->validateBaseUri();
  if (relativeBase.has_value()) {
    mBaseUri = UrlPlusRelative(mBaseUri, *relativeBase);
    this->validateBaseUri();
  }
}

std::shared_ptr<Client> HttpClient::Parameters::createBinaryClient() const {
  std::unique_ptr<Protocol::ClientParameters> parameters;
  if (mTls) {
    auto tls = std::make_unique<Tls::ClientParameters>(mIoContext, mEndPoint);
    tls->caCertFilePath(this->caCertFilepath());
    parameters = std::move(tls);
  }
  else {
    parameters = std::make_unique<Tcp::ClientParameters>(mIoContext, mEndPoint);
  }
  return Client::Create(*parameters, this->reconnectParameters());
}

void HttpClient::Parameters::validateBaseUri() const {
  if (!mBaseUri.has_scheme() || !mBaseUri.has_authority()) {
    throw std::runtime_error("HttpClient requires an absolute base URI");
  }
  if (mBaseUri.has_query()) {
    throw std::runtime_error("HttpClient base URI may not contain queries"); // You can add those to the HTTPRequest that you get from HttpClient::makeRequest
  }
  if (mBaseUri.host().empty()) {
    throw std::runtime_error("HttpClient base URI requires a host name");
  }
}

HttpClient::HttpClient(Parameters parameters)
  : mParameters(std::move(parameters)) {
}

void HttpClient::shutdown() {
  if (this->status() != Status::uninitialized && this->status() < Status::finalizing) {
    this->setStatus(Status::finalizing);
    this->stop();
  }
  this->setStatus(Status::finalized);
}

bool HttpClient::isRunning() const noexcept {
  auto status = this->status();
  return status > Status::uninitialized && status < Status::finalizing;
}

void HttpClient::start() {
  auto status = this->status();
  if (status > Status::initialized) {
    throw std::runtime_error("Can't (re)start an HttpClient after it has been shut down");
  }
  if (mBinaryClient != nullptr) {
    throw std::runtime_error("Can't start an HttpClient more than once");
  }
  this->setStatus(Status::initializing);

  mBinaryClient = mParameters.createBinaryClient();
  mBinaryClientConnectionAttempt = mBinaryClient->onConnectionAttempt.subscribe([weak = WeakFrom(*this)](const networking::Connection::Attempt::Result& result) {
    auto self = weak.lock();
    if (result && self != nullptr) {
      self->mConnection = *result; // TODO: clear when connection loses connectivity (and always check whether mConnection != nullptr before using it)
      self->setStatus(Status::initialized);
      self->ensureSend();
    }
    });
  mBinaryClient->start();
}

HTTPRequest HttpClient::makeRequest(HttpMethod method, const std::optional<std::string>& path) const {
  const auto& baseUri = mParameters.baseUri();
  return HTTPRequest(baseUri.host(), method, UrlPlusRelative(baseUri, path.value_or(std::string())), std::vector<std::shared_ptr<std::string>>(), std::map<std::string, std::string, CaseInsensitiveCompare>(), false);
}

std::string HttpClient::pathFromUrl(const boost::urls::url& full) {
  std::string str = full.c_str();
  std::string base = mParameters.baseUri().c_str();
  if (!str.starts_with(base)) {
    throw std::runtime_error("Client for " + base + " can't extract path from unrelated URL " + str);
  }
  return str.substr(base.size());
}

rxcpp::observable<HTTPResponse> HttpClient::sendRequest(HTTPRequest request) {
  if (!this->isRunning()) {
    throw std::runtime_error("HttpClient must be running to send a request");
  }
  if (!std::string(request.uri().c_str()).starts_with(mParameters.baseUri().c_str())) {
    throw std::runtime_error("Can't send request that doesn't match the HTTP client's base URI");
  }

  request.completeHeaders();
  auto sendable = MakeSharedCopy(std::move(request));
  onRequest.notify(sendable);

  return CreateObservable<HTTPResponse>([self = SharedFrom(*this), sendable](rxcpp::subscriber<HTTPResponse> subscriber) {
    self->mPendingRequests.push(MakeSharedCopy(PendingRequest{ sendable, subscriber }));

    // Stop (re)sending if/when the subscriber unsubscribes
    subscriber.add([self, sendable]() { self->unpend(sendable); });

    self->ensureSend();
    }).subscribe_on(observe_on_asio(mParameters.ioContext()));
}

void HttpClient::stop() {
  if (mBinaryClient != nullptr) {
    mBinaryClientConnectionAttempt.cancel();
    auto subscription = std::make_shared<EventSubscription>();
    if (this->status() == Status::finalizing) {
      // Notify that we've been finalized when the binary client becomes finalized
      *subscription = mBinaryClient->onStatusChange.subscribe([subscription, weak = WeakFrom(*this)](StatusChange change) {
        assert(change.updated >= Status::finalizing);
        auto self = weak.lock();
        if (self != nullptr && change.updated == Status::finalized) {
          assert(self->status() == Status::finalizing);
          self->setStatus(Status::finalized);
        }
        });
    }
    mBinaryClient->shutdown();
    mBinaryClient = nullptr;
  }
}

void HttpClient::restart() {
  this->setStatus(Status::initializing);
  this->stop();
  this->start();
}

bool HttpClient::continueSending(std::exception_ptr error) {
  if (error != nullptr) {
    LOG(LOG_TAG, debug) << "Error: " << GetExceptionMessage(error);

    // Reconnect to prevent the binary transport from remaining in a possibly invalid state
    // TODO: only do this if the binary transport didn't close (or reset) itself already
    this->restart();
    this->finishSending(error);
    return false;
  }

  if (mPendingRequests.empty() || mPendingRequests.front() != mSending) {
    this->finishSending();
    return false;
  }

  return true;
}

void HttpClient::ensureSend() {
  auto status = this->status();
  assert(status == Status::finalized || mBinaryClient != nullptr);
  if (mSending != nullptr || status >= Status::finalizing || mConnection == nullptr || !mConnection->isConnected()) {
    return;
  }

  // Don't send abandoned requests
  while (!mPendingRequests.empty() && !mPendingRequests.front()->subscriber.is_subscribed()) {
    mPendingRequests.pop();
  }
  if (mPendingRequests.empty()) {
    return;
  }

  mSending = mPendingRequests.front();
  mResponse = HTTPResponse();

  auto header = MakeSharedCopy(mSending->request->headerToString());
  mConnection->asyncWrite(header->data(), header->size(), [self = SharedFrom(*this), header](const SizedTransfer::Result& result) {
    self->handleRequestPartWritten(result, 0);
    });
}

bool HttpClient::unpend(std::shared_ptr<HTTPRequest> request) {
  if (!mPendingRequests.empty() && mPendingRequests.front()->request == request) {
    mPendingRequests.pop();
    return true;
  }

  return false;
}

void HttpClient::finishSending(std::exception_ptr error) {
  if (error == nullptr) {
    assert(mSending != nullptr);
    auto subscriber = mSending->subscriber;
    if (this->unpend(mSending->request)) { // Otherwise the subscriber has already unsubscribed
      subscriber.on_next(mResponse);
      subscriber.on_completed();
    }
  }
  // TODO: else notify subscriber of the error (they may wish to abort instead of having us retry)

  mSending.reset();
  this->ensureSend();
}

void HttpClient::handleRequestPartWritten(const SizedTransfer::Result& result, size_t sentBodyParts) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  const auto& parts = mSending->request->getBodyparts();
  for (auto i = sentBodyParts; i < parts.size(); ++i) {
    const auto& part = parts[i];
    if (!part->empty()) {
      mConnection->asyncWrite(part->data(), part->size(), [self = SharedFrom(*this), sent = i + 1](const SizedTransfer::Result& result) {
        self->handleRequestPartWritten(result, sent);
        });
      return;
    }
  }

  // Done sending body parts: start receiving the HTTPResponse
  mConnection->asyncReadUntil(CRLF, [self = SharedFrom(*this)](const DelimitedTransfer::Result& result) {
    //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) False positive in (type returned by) boost::is_any_of
    self->handleReadStatusLine(result);
    });
}

void HttpClient::handleReadStatusLine(const DelimitedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(result->ends_with(CRLF));
  std::istringstream responseStream(*result);

  std::string http_version;
  responseStream >> http_version;
  if (!http_version.starts_with("HTTP/")) {
    this->finishSending(std::make_exception_ptr(std::runtime_error("Invalid HTTP response: didn't start with required magic bytes")));
    return;
  }

  unsigned int statuscode{};
  responseStream >> statuscode;
  mResponse.setStatusCode(statuscode);

  std::string statusMessage;
  std::getline(responseStream, statusMessage);
  if (!responseStream) {
    this->finishSending(std::make_exception_ptr(std::runtime_error("Invalid HTTP response: status line unreadable")));
    return;
  }

  TrimOutsideWhitespace(statusMessage);
  mResponse.setStatusMessage(statusMessage);

  this->readHeaderLine();
}

void HttpClient::readHeaderLine() {
  mConnection->asyncReadUntil(CRLF, [self = SharedFrom(*this)](const DelimitedTransfer::Result& result) {
    self->handleReadHeaderLine(result);
    });
}

void HttpClient::handleReadHeaderLine(const DelimitedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(result->ends_with(CRLF));
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
      mResponse.setHeader(headerName, headerValue);
    }
    else {
      LOG(LOG_TAG, warning) << "Ignoring malformed header: " << header << '\n';
    }
    this->readHeaderLine(); // Done processing this header line: read the next one
  }
}

void HttpClient::readBody() {
  auto transferEncodingHeader = mResponse.getHeaders().find("Transfer-Encoding");
  if (transferEncodingHeader != mResponse.getHeaders().end()) {
    if (transferEncodingHeader->second.find("chunked") == std::string::npos) {
      // Since mBinaryClient may have received (or may still receive) stuff that we can't process, we can't (reliably) keep using it
      this->restart();
      this->finishSending(std::make_exception_ptr(std::runtime_error("Unsupported transfer encoding " + transferEncodingHeader->second)));
      return;
    }
    this->readChunkSize();
  }
  else {
    auto contentLengthHeader = mResponse.getHeaders().find("Content-Length");
    if (contentLengthHeader != mResponse.getHeaders().end()) {
      auto contentlength = boost::lexical_cast<size_t>(contentLengthHeader->second);
      if (contentlength > 0) {
        mContentBuffer.resize(contentlength);
        mConnection->asyncRead(mContentBuffer.data(), mContentBuffer.size(), [self = SharedFrom(*this)](const SizedTransfer::Result& result) {
          self->handleReadKnownSizeBody(result);
          });
      }
      else {
        this->finishSending();
      }
    }
    else {
      mConnection->asyncReadAll([self = SharedFrom(*this)](const DelimitedTransfer::Result& result) {
        self->handleReadConnectionBoundBody(result);
        });
    }
  }
}

void HttpClient::readChunkSize() {
  mConnection->asyncReadUntil(CRLF, [self = SharedFrom(*this)](const DelimitedTransfer::Result& result) {
    self->handleReadChunkSize(result);
    });
}

void HttpClient::handleReadChunkSize(const DelimitedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(boost::ends_with(*result, CRLF));
  std::istringstream responseStream(result->substr(0, result->size() - 2));
  size_t chunkSize{};
  responseStream >> std::hex >> chunkSize;

  if (chunkSize > 0) {
    // Read the chunk, including the trailing CRLF
    mContentBuffer.resize(chunkSize + 2);
    mConnection->asyncRead(mContentBuffer.data(), mContentBuffer.size(), [self = SharedFrom(*this)](const SizedTransfer::Result& result) {
      self->handleReadChunk(result);
      });
  }
  else {
    // We're processing the last (empty) chunk: read the CRLF that's written after its (empty) content
    mContentBuffer.resize(2);
    mConnection->asyncRead(mContentBuffer.data(), mContentBuffer.size(), [self = SharedFrom(*this)](const SizedTransfer::Result& result) {
      if (!self->continueSending(result.exception())) {
        return;
      }

      assert(self->mContentBuffer == CRLF);
      self->finishSending();
      });
  }
}

void HttpClient::handleReadChunk(const SizedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(boost::ends_with(mContentBuffer, CRLF));
  mContentBuffer.resize(mContentBuffer.size() - 2U);
  mResponse.getBodyparts().push_back(MakeSharedCopy(mContentBuffer));

  this->readChunkSize();
}

void HttpClient::handleReadKnownSizeBody(const SizedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }

  assert(*result == mContentBuffer.size());
  this->handleReadBody(std::move(mContentBuffer));
}

void HttpClient::handleReadConnectionBoundBody(const DelimitedTransfer::Result& result) {
  if (!this->continueSending(result.exception())) {
    return;
  }
  this->handleReadBody(*result);
}

void HttpClient::handleReadBody(std::string body) {
  mResponse.getBodyparts().push_back(MakeSharedCopy(std::move(body)));
  this->finishSending();
}

}
