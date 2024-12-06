#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>

#include <rxcpp/rx-lite.hpp>

#include <pep/castor/tests/FakeCastorApi.hpp>
#include <pep/application/Application.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Paths.hpp>
#include <pep/networking/TLSClient.hpp>

namespace pep {
namespace castor {

namespace {

const std::string LOG_TAG ("FakeCastorApi");

std::filesystem::path GetIdentityFilePath(const std::string& name) {
  return GetAbsolutePath(name);
}

}

void FakeCastorApi::Connection::onConnectSuccess() {
  this->mState = TLSProtocol::Connection::CONNECTED;
  boost::asio::async_read_until(*socket, buffer, "\r\n",
                                boost::bind(&FakeCastorApi::Connection::handleReadRequestLine,
                                  SharedFrom(*this),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred));
}

void FakeCastorApi::Connection::onError(const boost::system::error_code& error) {
  LOG(LOG_TAG, info) << error.message();
  writeOutput("400 Bad Request", "400 Bad Request");
}

void FakeCastorApi::Connection::handleReadRequestLine(const boost::system::error_code& error, size_t bytes) {
  if (error) {
    onError(error);
    return;
  }

  std::istream responseStream(&buffer);
  std::string httpVersion;
  responseStream >> method;
  responseStream >> path;
  std::getline(responseStream, httpVersion);

  boost::asio::async_read_until(*socket, buffer, "\r\n\r\n",
                                boost::bind(&FakeCastorApi::Connection::handleReadHeaders,
                                  SharedFrom(*this),
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred));
}

void FakeCastorApi::Connection::handleReadHeaders(const boost::system::error_code& error, size_t bytes) {
  if (error) {
    onError(error);
    return;
  }

  std::istream responseStream(&buffer);
  std::string header;
  while (std::getline(responseStream, header) && header != "\r") {
    size_t index = header.find(':');
    if(index != std::string::npos) {
      std::string headerName = header.substr(0, index);
      std::string headerValue = header.substr(index + 1);
      boost::trim_if(headerValue, boost::is_any_of(" \r\n\t"));
      headers[headerName] = headerValue;
    } else {
      LOG(LOG_TAG, warning) << "Ignoring malformed header: " << header << '\n';
    }
  }

  auto contentLengthHeader = headers.find("Content-Length");
  if (contentLengthHeader != headers.end()) {
    contentlength = boost::lexical_cast<size_t>(contentLengthHeader->second);
  } else {
    contentlength = 0;
  }

  if (contentlength > 0) {
    boost::asio::async_read(*socket, buffer, boost::asio::transfer_exactly(contentlength - buffer.size()),
                            boost::bind(&FakeCastorApi::Connection::handleReadBody,
                              SharedFrom(*this),
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));
  } else {
    handleRequest();
  }
}

void FakeCastorApi::Connection::handleReadBody(const boost::system::error_code& error, size_t bytes) {
  if (error) {
    onError(error);
    return;
  }

  std::istream responseStream(&buffer);
  body = std::string(contentlength, '\0');
  responseStream.read(&body[0], static_cast<std::streamsize>(contentlength));

  handleRequest();
}

void FakeCastorApi::Connection::handleRequest() {
  LOG(LOG_TAG, debug) << "Received request: " << method << " " << path << std::endl << body;

  if(path.starts_with("/api/")) {
    auto authzHeader = headers.find("Authorization");
    if(authzHeader == headers.end() || authzHeader->second != "Bearer f74ffb4d8a4c9a0a3992836357d668bee1231172") {
      writeOutput("{\"type\":\"http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html\",\"title\":\"Forbidden\",\"status\":403,\"detail\":\"You are not authorized to view this study.\"}", "403 Forbidden");
      return;
    }
  }

  auto options = getServer()->mOptions;
  auto findResponse = options->responses.find(path);
  if(findResponse != options->responses.end()) {
    writeOutput(boost::algorithm::replace_all_copy(findResponse->second.body, "[URL]", getUrl()), findResponse->second.status);
  } else if(method == "POST" && path == "/oauth/token") {
    if(options->authenticated) {
      writeOutput("{\"access_token\":\"f74ffb4d8a4c9a0a3992836357d668bee1231172\",\"expires_in\":18000,\"token_type\":\"Bearer\",\"scope\":\"1\"}");
    } else {
      writeOutput("{\"error\":\"invalid_client\",\"error_description\":\"The client credentials are invalid\"}", "400 Bad Request");
    }
  } else {
    writeOutput("Bad Request", "400 Bad Request");
  }
}

void FakeCastorApi::Connection::handleWrite(const boost::system::error_code& error, size_t bytes) {
  if (error) {
    LOG(LOG_TAG, warning) << "Error while writing response: " << error.message();
    return;
  }

  LOG(LOG_TAG, debug) << "Written output " << output.substr(0, bytes);
}

void FakeCastorApi::Connection::writeOutput(const std::string& body, const std::string& status, std::map<std::string, std::string> responseHeaders) {
  responseHeaders["Content-Length"] = std::to_string(body.length());

  output = "HTTP/1.1 " + status + "\r\n";
  for(const auto& header : responseHeaders) {
    output += header.first + ": " + header.second + "\r\n";
  }
  output += "\r\n";
  output += body;
  boost::asio::async_write(*socket, boost::asio::buffer(&output[0], output.length()),
                           boost::bind(&FakeCastorApi::Connection::handleWrite, SharedFrom(*this),
                                       boost::asio::placeholders::error,
                                       boost::asio::placeholders::bytes_transferred));
}

FakeCastorApi::Parameters::Parameters()
  : TLSServer<TLSProtocol>::Parameters(X509IdentityFilesConfiguration(GetIdentityFilePath("localhost.key"), GetIdentityFilePath("localhost.cert"))) {
}

FakeCastorTest::FakeCastorTest()
  : io_service(std::make_shared<boost::asio::io_service>()), parameters(std::make_shared<FakeCastorApi::Parameters>()) {
  parameters->setIOservice(std::make_shared<boost::asio::io_service>());

  options = std::make_shared<FakeCastorApi::Options>();
  parameters->options = options;

  constexpr uint16_t fakeCastorPort{0xca5};  // 'CAS'(tor), just some arbitrary port
  port = fakeCastorPort;
  parameters->setListenPort(port);

  listener = TLSListener<FakeCastorApi>::Create(parameters);

  castorConnection = castor::CastorConnection::Create(
    EndPoint("localhost", port),
    castor::ApiKey{ "SomeID", "SomeSecret" },
    io_service,
    GetIdentityFilePath("localhost.cert"));

  io_service_thread = IOServiceThread(io_service, &runIOservice);
  fakeCastorThread = IOServiceThread(parameters->getIOservice(), &runFakeCastor);
  LOG(LOG_TAG, info) << "FakeCastorApi listening on port " << port;
}

void FakeCastorTest::TearDown() {
  runIOservice = false;
  io_service->stop();
  io_service_thread.join();

  runFakeCastor = false;
  parameters->getIOservice()->stop();
  fakeCastorThread.join();
}
}
}
