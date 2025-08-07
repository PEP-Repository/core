#pragma once

#include <gtest/gtest.h>

#include <pep/castor/CastorConnection.hpp>
#include <pep/async/IoContextThread.hpp>
#include <pep/networking/TLSServer.hpp>

#include <boost/asio/streambuf.hpp>

namespace pep {
namespace castor {

class FakeCastorApi : public TLSServer<TLSProtocol> {
 public:
  struct Response {
    std::string body;
    std::string status;

    Response (const std::string& body, const std::string& status = "200 OK")
      : body(body), status(status) {}
  };
  struct Options {
    bool authenticated = true;
    std::map<std::string, Response> responses;
  };

  class Parameters : public TLSServer<TLSProtocol>::Parameters {
   public:
    Parameters();
    std::shared_ptr<Options> options;
  };

  class Connection : public TLSServer<TLSProtocol>::Connection {
  public:
    Connection(std::shared_ptr<FakeCastorApi> server)
      : TLSServer<TLSProtocol>::Connection(server) {
    }

  protected:
    std::shared_ptr<FakeCastorApi> getServer() const noexcept { return std::static_pointer_cast<FakeCastorApi>(TLSServer<TLSProtocol>::Connection::getServer()); }

  private:
    void onConnectSuccess() override;
    void onError(const boost::system::error_code& error);
    void handleReadRequestLine(const boost::system::error_code& error, size_t bytes);
    void handleReadHeaders(const boost::system::error_code& error, size_t bytes);
    void handleReadBody(const boost::system::error_code& error, size_t bytes);
    void handleRequest();
    void handleWrite(const boost::system::error_code& error, size_t bytes);
    void writeOutput(const std::string& body, const std::string& status = "200 OK", std::map<std::string, std::string> responseHeaders = std::map<std::string, std::string>());
    std::string getUrl() {
      return "https://localhost:" + std::to_string(getServer()->getListenPort());
    }

    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
    size_t contentlength{};
    boost::asio::streambuf buffer;
    std::string output;
  };

public:
  explicit FakeCastorApi(std::shared_ptr<Parameters> parameters) : TLSServer<TLSProtocol>(parameters), mOptions(parameters->options) {}

protected:
  std::string describe() const override {
    return "Fake Castor";
  }

private:
  std::shared_ptr<Options> mOptions;
};

class FakeCastorTest : public ::testing::Test {
 protected:
  FakeCastorTest();
  void TearDown() override;

  uint16_t port;
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<CastorConnection> castorConnection;
  IoContextThread io_context_thread;
  bool runIoContext = true;
  IoContextThread fakeCastorThread;
  bool runFakeCastor = true;
  std::shared_ptr<FakeCastorApi::Parameters> parameters;
  std::shared_ptr<TLSListener<FakeCastorApi>> listener;
  std::shared_ptr<FakeCastorApi::Options> options;
};

}
}
