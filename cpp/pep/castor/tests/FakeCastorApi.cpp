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

const std::string LOG_TAG ("FakeCastorApi");

std::filesystem::path GetIdentityFilePath(const std::string& name) {
  return GetAbsolutePath(name);
}

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
    return "https://localhost:" + std::to_string(mServer->getListenPort());
  };

  std::shared_ptr<FakeCastorApi> mServer;
  std::shared_ptr<networking::Connection> mBinary;
  std::string mMethod;
  std::string mPath;
  std::map<std::string, std::string> mHeaders;
  std::string mBody;
  size_t mContentlength = 0U;
  std::string mOutput;
};

FakeCastorApi::Connection::Connection(std::shared_ptr<FakeCastorApi> server, std::shared_ptr<networking::Connection> binary)
  : mServer(server), mBinary(binary) {
}

void FakeCastorApi::Connection::acceptMessage() {
  mBinary->asyncReadUntil("\r\n", [self = SharedFrom(*this)](const networking::DelimitedTransfer::Result& result) { self->handleReadRequestLine(result); });
}

void FakeCastorApi::Connection::handleError(std::exception_ptr error) {
  LOG(LOG_TAG, info) << GetExceptionMessage(error);
  writeOutput("400 Bad Request", "400 Bad Request");
}

void FakeCastorApi::Connection::handleReadRequestLine(const networking::DelimitedTransfer::Result& result) {
  if (!result) {
    this->handleError(result.exception());
    return;
  }

  std::istringstream responseStream(*result);
  std::string httpVersion;
  responseStream >> mMethod;
  responseStream >> mPath;
  std::getline(responseStream, httpVersion);

  mBinary->asyncReadUntil("\r\n\r\n", [self = SharedFrom(*this)](const networking::DelimitedTransfer::Result& result) {self->handleReadHeaders(result); });
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
      boost::trim_if(headerValue, boost::is_any_of(" \r\n\t"));
      mHeaders[headerName] = headerValue;
    } else {
      LOG(LOG_TAG, warning) << "Ignoring malformed header: " << header << '\n';
    }
  }

  assert(mContentlength == 0);
  auto contentLengthHeader = mHeaders.find("Content-Length");
  if (contentLengthHeader != mHeaders.end()) {
    mContentlength = boost::lexical_cast<size_t>(contentLengthHeader->second);
  }

  if (mContentlength > 0) {
    mBody.resize(mContentlength);
    mBinary->asyncRead(mBody.data(), mContentlength, [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {self->handleReadBody(result); });
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
  LOG(LOG_TAG, debug) << "Received request: " << mMethod << " " << mPath << std::endl << mBody;

  if(mPath.starts_with("/api/")) {
    auto authzHeader = mHeaders.find("Authorization");
    if(authzHeader == mHeaders.end() || authzHeader->second != "Bearer f74ffb4d8a4c9a0a3992836357d668bee1231172") {
      writeOutput("{\"type\":\"http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html\",\"title\":\"Forbidden\",\"status\":403,\"detail\":\"You are not authorized to view this study.\"}", "403 Forbidden");
      return;
    }
  }

  auto options = mServer->mOptions;
  auto findResponse = options->responses.find(mPath);
  if(findResponse != options->responses.end()) {
    writeOutput(boost::algorithm::replace_all_copy(findResponse->second.body, "[URL]", getUrl()), findResponse->second.status);
  } else if(mMethod == "POST" && mPath == "/oauth/token") {
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
    LOG(LOG_TAG, warning) << "Error while writing response: " << GetExceptionMessage(result.exception());
    return;
  }

  LOG(LOG_TAG, debug) << "Written output " << mOutput.substr(0, *result);
}

void FakeCastorApi::Connection::writeOutput(const std::string& body, const std::string& status, std::map<std::string, std::string> responseHeaders) {
  responseHeaders["Content-Length"] = std::to_string(body.length());

  mOutput = "HTTP/1.1 " + status + "\r\n";
  for(const auto& header : responseHeaders) {
    mOutput += header.first + ": " + header.second + "\r\n";
  }
  mOutput += "\r\n";
  mOutput += body;
  
  mBinary->asyncWrite(mOutput.data(), mOutput.length(), [self = SharedFrom(*this)](const networking::SizedTransfer::Result& result) {self->handleWrite(result); });
}

FakeCastorTest::FakeCastorTest() {
  constexpr uint16_t port{ 0xca5 };  // 'CAS'(tor), just some arbitrary port
  networking::Tls::ServerParameters parameters(*mServerSide.ioContext(), port, X509IdentityFilesConfiguration(GetIdentityFilePath("localhost.key"), GetIdentityFilePath("localhost.cert")));
  mServer = FakeCastorApi::Create(parameters, port, options);
  mServer->start();

  castorConnection = castor::CastorConnection::Create(
    EndPoint("localhost", port),
    castor::ApiKey{ "SomeID", "SomeSecret" },
    mClientSide.ioContext(),
    GetIdentityFilePath("localhost.cert"));

  mClientSide.start();
  mServerSide.start();
  LOG(LOG_TAG, info) << "FakeCastorApi listening on port " << port;
}

void FakeCastorTest::TearDown() {
  castorConnection = nullptr;
  mClientSide.stop(false);

  mServer->stop();
  mServerSide.stop(false);
}

void FakeCastorTest::Side::start() {
  if (mThread != nullptr) {
    throw std::runtime_error("Can't start FakeCastorTest::Side multiple times");
  }
  mThread = std::make_shared<IoContextThread>(mIoContext, &mRun);
}

void FakeCastorTest::Side::stop(bool force) {
  if (!mRun) {
    throw std::runtime_error("Can't stop FakeCastorTest::Side multiple times");
  }
  if (mThread == nullptr) {
    throw std::runtime_error("Can't stop an unstarted FakeCastorTest::Side");
  }
  mRun = false; // Don't restart the I/O service if/when it runs out of work
  if (force) {
    mIoContext->stop();
  }
  mThread->join();
}

FakeCastorApi::FakeCastorApi(const pep::networking::Protocol::ServerParameters& parameters, uint16_t port, std::shared_ptr<Options> options)
  : mOptions(options), mConnectivity(networking::Server::Create(parameters)), mPort(port) {
}

void FakeCastorApi::start() {
  mConnectivityConnectionAttempt = mConnectivity->onConnectionAttempt.subscribe([self = SharedFrom(*this)](const networking::Connection::Attempt::Result& result) {
    if (!*result) {
      LOG(LOG_TAG, warning) << "Incoming Fake Castor API connection failed: " << GetExceptionMessage(result.exception());
    }
    else {
      Connection::Create(self, *result)->acceptMessage();
    }
    });
  mConnectivity->start();
}

void FakeCastorApi::stop() {
  mConnectivityConnectionAttempt.cancel();
  mConnectivity->shutdown();
}

}
}
