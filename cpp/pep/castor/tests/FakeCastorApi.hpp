#pragma once

#include <gtest/gtest.h>

#include <pep/castor/CastorConnection.hpp>
#include <pep/async/IoContextThread.hpp>
#include <pep/networking/Server.hpp>
#include <pep/crypto/tests/TemporaryX509IdentityFiles.hpp>

#include <boost/asio/streambuf.hpp>

namespace pep {
namespace castor {

class FakeCastorApi : public std::enable_shared_from_this<FakeCastorApi>, public SharedConstructor<FakeCastorApi> {
  friend class SharedConstructor<FakeCastorApi>;

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

  void start();
  void stop();

private:
  class Connection;

  explicit FakeCastorApi(const pep::networking::Protocol::ServerParameters& parameters, uint16_t port, std::shared_ptr<Options> options);
  uint16_t getListenPort() const noexcept { return mPort; }

  std::shared_ptr<Options> mOptions;
  std::shared_ptr<networking::Server> mConnectivity;
  EventSubscription mConnectivityConnectionAttempt;
  uint16_t mPort;
};


class FakeCastorTest : public ::testing::Test {
protected:
  FakeCastorTest();
  void TearDown() override;

  std::shared_ptr<CastorConnection> castorConnection;
  std::shared_ptr<FakeCastorApi::Options> options = std::make_shared<FakeCastorApi::Options>();

private:
  class Side : boost::noncopyable {
  private:
    std::shared_ptr<boost::asio::io_context> mIoContext = std::make_shared< boost::asio::io_context>();
    bool mRun = true;
    std::shared_ptr<IoContextThread> mThread;

  public:
    std::shared_ptr<boost::asio::io_context> ioContext() const noexcept { return mIoContext; }

    void start();
    void stop(bool force = true);
  };

  TemporaryX509IdentityFiles mIdentity;
  Side mClientSide;
  Side mServerSide;
  std::shared_ptr<FakeCastorApi> mServer;
};

}
}
