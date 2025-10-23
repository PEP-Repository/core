#include <pep/networking/Server.hpp>
#include <pep/networking/Tcp.hpp>
#include <pep/utils/TestTiming.hpp>

#include <boost/asio/steady_timer.hpp>
#include <gtest/gtest.h>

using namespace std::literals;

namespace {

class FakeProtocol : public pep::networking::ProtocolImplementor<FakeProtocol> {
  using Base = pep::networking::ProtocolImplementor<FakeProtocol>;

public:
  class Socket : public Base::Socket, public pep::SharedConstructor<Socket> {
    friend class pep::SharedConstructor<Socket>;

  private:
    static size_t instances_;
    static size_t unclosed_;

    explicit Socket(boost::asio::io_context& ioContext) noexcept : Base::Socket(FakeProtocol::Instance(), ioContext) {
      ++instances_;
      ++unclosed_;
    }

  public:
    ~Socket() noexcept override { --instances_; }

    std::string remoteAddress() const override { return "fake remote node"; }
    void close() override { --unclosed_; }

    void asyncRead(void* destination, size_t bytes, const pep::networking::SizedTransfer::Handler& onTransferred) override { throw std::runtime_error("Not implemented"); }
    void asyncReadUntil(const char* delimiter, const pep::networking::DelimitedTransfer::Handler& onTransferred) override { throw std::runtime_error("Not implemented"); }
    void asyncReadAll(const pep::networking::DelimitedTransfer::Handler& onTransferred) override { throw std::runtime_error("Not implemented"); }
    void asyncWrite(const void* source, size_t bytes, const pep::networking::SizedTransfer::Handler& onTransferred) override { throw std::runtime_error("Not implemented"); }

    static size_t Instances() noexcept { return instances_; }
    static size_t Unclosed() noexcept { return unclosed_; }
  };

  class ServerParameters : public Base::ServerParameters {
  public:
    explicit ServerParameters(boost::asio::io_context& ioContext) noexcept : Base::ServerParameters(FakeProtocol::Instance(), ioContext) {}
    std::string addressSummary() const override { return "fake server address"; }
  };

  class ClientParameters : public Base::ClientParameters {
  public:
    explicit ClientParameters(boost::asio::io_context& ioContext) noexcept : Base::ClientParameters(FakeProtocol::Instance(), ioContext) {}
    std::string addressSummary() const override { return "fake client address"; }
  };

  class ServerComponent : public Base::ServerComponent {
  public:
    explicit ServerComponent(const ServerParameters& parameters) : Base::ServerComponent(parameters) {}
    void close() override {}
    std::shared_ptr<Protocol::Socket> openSocket(const ConnectionAttempt::Handler& notify) override { return Socket::Create(this->ioContext()); }
  };

  class ClientComponent : public Base::ClientComponent {
  public:
    explicit ClientComponent(const ClientParameters& parameters) : Base::ClientComponent(parameters) {}
    void close() override {}
    std::shared_ptr<Protocol::Socket> openSocket(const ConnectionAttempt::Handler& notify) override { return Socket::Create(this->ioContext()); }
  };

  std::string name() const override { return "fake"; }
  std::shared_ptr<Base::ClientParameters> createClientParameters(const Base::ServerComponent& server) const override { return std::make_shared<ClientParameters>(server.downcastFor(*this).ioContext()); }
};

size_t FakeProtocol::Socket::instances_ = 0U;
size_t FakeProtocol::Socket::unclosed_ = 0U;

}


TEST(Server, DiscardsUnopenedSocket) {
  EXPECT_EQ(0U, FakeProtocol::Socket::Instances()) << "Can't reliably count sockets. Are other (concurrently executed) tests using FakeProtocol as well?";
  EXPECT_EQ(0U, FakeProtocol::Socket::Instances()) << "Can't reliably count unclosed sockets. Are other (concurrently executed) tests using FakeProtocol as well?";

  {
    boost::asio::io_context context;
    auto server = pep::networking::Server::Create(FakeProtocol::ServerParameters(context));
    server->start();

    EXPECT_EQ(1U, FakeProtocol::Socket::Instances()) << "Expected server to open a socket when started";
    EXPECT_EQ(1U, FakeProtocol::Socket::Unclosed()) << "Expected socket to be open while server is running";
  }

  EXPECT_EQ(0U, FakeProtocol::Socket::Unclosed()) << "Server didn't close socket";
  EXPECT_EQ(0U, FakeProtocol::Socket::Instances()) << "Server didn't discard its socket(s) upon destruction";
}

TEST(Server, UnschedulesOnDestruction) {
  auto SHORT_TIME = 100ms;
  auto LONG_TIME = 200ms;

  boost::asio::io_context context;

  auto server = pep::networking::Server::Create(pep::networking::Tcp::ServerParameters(context, pep::networking::Tcp::ServerParameters::RANDOM_PORT));
  // Don't subscribe to server->onConnectionAttempt: this test just wants to verify what happens when the server is destroyed
  server->start();

  // Release our shared_ptr to the server after SHORT_DURATION
  boost::asio::steady_timer timer(context);
  timer.expires_after(SHORT_TIME);
  timer.async_wait([&server /* note: captured by reference */](const boost::system::error_code& error) {
    server.reset(); // Ensure that the server is discarded even if our test assertion doesn't hold, preventing said server from keeping the I/O context busy
    ASSERT_FALSE(error) << "Timer produced an error: " << error;
    });

  // Have the I/O context run for at most LONG_DURATION, and measure how long it runs
  auto started = pep::testing::Clock::now();
  context.run_for(LONG_TIME);
  auto duration = pep::testing::MillisecondsSince(started);

  // If the server unscheduled all its work when it was destroyed, the I/O context will have stopped running at that moment
  ASSERT_GE(duration, SHORT_TIME) << "I/O context finished before server was discarded";
  ASSERT_LT(duration, LONG_TIME) << "I/O server kept running after server was discarded";
}
