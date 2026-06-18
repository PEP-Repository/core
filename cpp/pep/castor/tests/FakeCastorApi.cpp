#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>

#include <rxcpp/rx-lite.hpp>

#include <pep/castor/tests/FakeCastorApi.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Paths.hpp>
#include <pep/networking/Tls.hpp>
#include <pep/utils/Exceptions.hpp>

namespace pep {
namespace castor {

namespace {

const std::string LogTag ("FakeCastorApi");

}

class FakeCastorApi::Connection : public std::enable_shared_from_this<Connection>, public SharedConstructor<Connection> {
  friend class SharedConstructor<Connection>;

private:
  Connection(std::shared_ptr<FakeCastorApi> server, std::shared_ptr<networking::Connection> binary);

public:
  void acceptMessage(); // Connection keeps itself alive until the message has been handled

private:
  void handleError(std::exception_ptr error);
  void handleReadRequestLine(const networking::DelimitedTransfer::Result& result);
  void handleReadHeaders(const networking::DelimitedTransfer::Result& result);
  void handleReadBody(const networking::SizedTransfer::Result& result);
  void handleRequest();
  void handleWrite(const networking::SizedTransfer::Result& result);
  void writeOutput(const std::string& body, const std::string& status = "200 OK", std::map<std::string, std::string> responseHeaders = std::map<std::string, std::string>());
  std::string getUrl() {
    return "https://localhost:" + std::to_string(server_->getListenPort());
  };

  std::shared_ptr<FakeCastorApi> server_;
  std::shared_ptr<networking::Connection> binary_;
  std::string method_;
  std::string path_;
  std::map<std::string, std::string> headers_;
  std::string body_;
  size_t contentlength_ = 0U;
  std::string output_;
};

FakeCastorApi::Connection::Connection(std::shared_ptr<FakeCastorApi> server, std::shared_ptr<networking::Connection> binary)
  : server_(server), binary_(binary) {
}

void FakeCastorApi::Connection::acceptMessage() {
  binary_->asyncReadUntil("\r\n", [self = SharedFrom(*this)](const networking::DelimitedTransfer::Result& result) { self->handleReadRequestLine(result); });
}

void FakeCastorApi::Connection::handleError(std::exception_ptr error) {
  PEP_LOG(LogTag, Severity::Info) << GetExceptionMessage(error);
  writeOutput("400 Bad Request", "400 Bad Request");
}

void FakeCastorApi::Connection::handleReadRequestLine(const networking::DelimitedTransfer::Result& result) {
  if (!result) {
    this->handleError(result.exception());
    return;
  }

  std::istringstream responseStream(*result);
  std::string httpVersion;
  responseStream >> method_;
  responseStream >> path_;
  std::getline(responseStream, httpVersion);

  binary_->asyncReadUntil("\r\n\r\n", [self = SharedFrom(*this)](const networking::DelimitedTransfer::Result& result) {self->handleReadHeaders(result); });
}

void FakeCastorApi::Connection::handleReadHeaders(const networking::DelimitedTransfer::Result& result) {
  if (!result) {
    this->handleError(result.exception());
    return;
  }

  std::istringstream responseStream(*result);
  std::string header;
  while (std::getline(responseStream, header) && header != "\r") {
    size_t index = header.find(':');
    if(index != std::string::npos) {
      std::string headerName = header.substr(0, index);
      std::string headerValue = header.substr(index + 1);
      boost::trim_if(headerValue, boost::is_space());
      headers_[headerName] = headerValue;
    } else {
      PEP_LOG(LogTag, Severity::Warning) << "Ignoring malformed header: " << header << '\n';
    }
  }

  assert(contentlength_ == 0);
  auto contentLengthHeader = headers_.find("Content-Length");
  if (contentLengthHeader != headers_.end()) {
    contentlength_ = boost::lexical_cast<size_t>(contentLengthHeader->second);
  }

  if (contentlength_ > 0) {
    body_.resize(contentlength_);
    binary_->asyncRead(body_.data(), contentlength_, [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {self->handleReadBody(result); });
  } else {
    handleRequest();
  }
}

void FakeCastorApi::Connection::handleReadBody(const networking::SizedTransfer::Result& result) {
  if (!result) {
    this->handleError(result.exception());
    return;
  }
  handleRequest();
}

void FakeCastorApi::Connection::handleRequest() {
  PEP_LOG(LogTag, Severity::Debug) << "Received request: " << method_ << " " << path_ << std::endl << body_;

  if(path_.starts_with("/api/")) {
    auto authzHeader = headers_.find("Authorization");
    if(authzHeader == headers_.end() || authzHeader->second != "Bearer f74ffb4d8a4c9a0a3992836357d668bee1231172") {
      writeOutput("{\"type\":\"http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html\",\"title\":\"Forbidden\",\"status\":403,\"detail\":\"You are not authorized to view this study.\"}", "403 Forbidden");
      return;
    }
  }

  auto options = server_->options_;
  auto findResponse = options->responses.find(path_);
  if(findResponse != options->responses.end()) {
    writeOutput(boost::algorithm::replace_all_copy(findResponse->second.body, "[URL]", getUrl()), findResponse->second.status);
  } else if(method_ == "POST" && path_ == "/oauth/token") {
    if(options->authenticated) {
      writeOutput("{\"access_token\":\"f74ffb4d8a4c9a0a3992836357d668bee1231172\",\"expires_in\":18000,\"token_type\":\"Bearer\",\"scope\":\"1\"}");
    } else {
      writeOutput("{\"error\":\"invalid_client\",\"error_description\":\"The client credentials are invalid\"}", "400 Bad Request");
    }
  } else {
    writeOutput("Bad Request", "400 Bad Request");
  }
}

void FakeCastorApi::Connection::handleWrite(const networking::SizedTransfer::Result& result) {
  if (!result) {
    PEP_LOG(LogTag, Severity::Warning) << "Error while writing response: " << GetExceptionMessage(result.exception());
    return;
  }

  PEP_LOG(LogTag, Severity::Debug) << "Written output " << output_.substr(0, *result);
}

void FakeCastorApi::Connection::writeOutput(const std::string& body, const std::string& status, std::map<std::string, std::string> responseHeaders) {
  responseHeaders["Content-Length"] = std::to_string(body.length());

  output_ = "HTTP/1.1 " + status + "\r\n";
  for(const auto& header : responseHeaders) {
    output_ += header.first + ": " + header.second + "\r\n";
  }
  output_ += "\r\n";
  output_ += body;
  
  binary_->asyncWrite(output_.data(), output_.length(), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {self->handleWrite(result); });
}

FakeCastorTest::FakeCastorTest()
  : identity_(TemporaryX509IdentityFiles::Make("Fake Castor b.v.", "localhost")) { // Would be cheaper in a (global) RegisteredTestEnvironment, but this is good enough for now
  constexpr uint16_t port{ 0xca5 };  // 'CAS'(tor), just some arbitrary port
  networking::Tls::ServerParameters parameters(*serverSide_.ioContext(), port, identity_.slicedToX509IdentityFiles());
  server_ = FakeCastorApi::Create(parameters, port, options);
  server_->start();

  castorConnection = castor::CastorConnection::Create(
    EndPoint("localhost", port),
    castor::ApiKey{ "SomeID", "SomeSecret" },
    clientSide_.ioContext(),
    identity_.getCertificateChainFilePath());

  clientSide_.start();
  serverSide_.start();
  PEP_LOG(LogTag, Severity::Info) << "FakeCastorApi listening on port " << port;
}

void FakeCastorTest::TearDown() {
  castorConnection = nullptr;
  clientSide_.stop(false);

  server_->stop();
  serverSide_.stop(false);
}

void FakeCastorTest::Side::start() {
  if (thread_ != nullptr) {
    throw std::runtime_error("Can't start FakeCastorTest::Side multiple times");
  }
  thread_ = std::make_shared<IoContextThread>("Fake Castor test " + this->role(), ioContext_);
}

void FakeCastorTest::Side::stop(bool force) {
  if (ioContext_ == nullptr) {
    throw std::runtime_error("Can't stop FakeCastorTest::Side multiple times");
  }
  if (thread_ == nullptr) {
    throw std::runtime_error("Can't stop an unstarted FakeCastorTest::Side");
  }
  ioContext_.reset(); // Allow detection that this instance has already been stop()ped
  thread_->stop(force);
}

FakeCastorApi::FakeCastorApi(const pep::networking::Protocol::ServerParameters& parameters, uint16_t port, std::shared_ptr<Options> options)
  : options_(options), connectivity_(networking::Server::Create(parameters)), port_(port) {
}

void FakeCastorApi::start() {
  connectivityConnectionAttempt_ = connectivity_->onConnectionAttempt.subscribe([self = SharedFrom(*this)](const networking::Connection::Attempt::Result& result) {
    if (!*result) {
      PEP_LOG(LogTag, Severity::Warning) << "Incoming Fake Castor API connection failed: " << GetExceptionMessage(result.exception());
    }
    else {
      Connection::Create(self, *result)->acceptMessage();
    }
    });
  connectivity_->start();
}

void FakeCastorApi::stop() {
  connectivityConnectionAttempt_.cancel();
  connectivity_->shutdown();
}

}
}
