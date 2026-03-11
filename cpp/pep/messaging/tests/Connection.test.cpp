#include <pep/async/RxFinallyExhaust.hpp>
#include <pep/messaging/Node.hpp>
#include <pep/networking/tests/TestServerFactory.test.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <gtest/gtest.h>

namespace {

void TestConnectionBasics(TestServerFactory& factory) {
  boost::asio::io_context io_context;
  pep::messaging::RequestHandler handler;

  auto protocol = factory.protocol().name();
  auto serverParameters = factory.createServerParameters(io_context, 2022); // TODO: support port randomization?
  auto server = pep::messaging::Node::Create(*serverParameters, handler);
  auto client = pep::messaging::Node::Create(*factory.createClientParameters(*serverParameters));

  server->start()
    .concat_map([server, protocol](const pep::messaging::Connection::Attempt::Result& result) {
    std::cerr << protocol << " server connection attempt" << std::endl;
    EXPECT_TRUE(result) << protocol << " server connection attempt failed";
    if (result) {
      auto connection = *result;
      EXPECT_EQ(connection->status(), pep::messaging::Connection::Status::initialized) << protocol << " server produced non-initialized connection";
    }
    return server->shutdown();
      })
    .op(pep::RxFinallyExhaust(io_context, [server]() { return server->shutdown(); })) // Ensure that the server is shut down even if exceptions are raised
    .subscribe(
      [protocol](pep::FakeVoid) {
        std::cerr << protocol << " server shutdown result" << std::endl;
      },
      [server, protocol](std::exception_ptr error) {
        std::cerr << protocol << " server connectivity error" << pep::GetExceptionMessage(error) << std::endl;
        FAIL() << protocol << " server connectivity produced an error: " << pep::GetExceptionMessage(error);
      },
      [protocol]() {
        std::cerr << protocol << " server shutdown complete" << std::endl;
      }
    );

  client->start()
    .concat_map(
      [client, protocol](const pep::messaging::Connection::Attempt::Result& result) {
        std::cerr << protocol << " client connection attempt" << std::endl;
        EXPECT_TRUE(result) << protocol << " client connection attempt failed";
        if (result) {
          auto connection = *result;
          EXPECT_EQ(connection->status(), pep::messaging::Connection::Status::initialized) << protocol << " client produced non-initialized connection";
        }
        return client->shutdown();
      })
    .op(pep::RxFinallyExhaust(io_context, [client]() { return client->shutdown(); })) // Ensure that the server is shut down even if exceptions are raised
    .subscribe(
      [protocol](pep::FakeVoid) {
        std::cerr << protocol << " client shutdown result" << std::endl;
      },
      [client, protocol](std::exception_ptr error) {
        std::cerr << protocol << " client connectivity error" << pep::GetExceptionMessage(error) << std::endl;
        FAIL() << protocol << " client connectivity produced an error: " << pep::GetExceptionMessage(error);
      },
      [protocol]() {
        std::cerr << protocol << " client shutdown complete" << std::endl;
      }
    );

  ASSERT_NO_FATAL_FAILURE(io_context.run());
}

} // End anonymous namespace

TEST(Connection, Basics) {
  TcpTestServerFactory tcp;
  TestConnectionBasics(tcp);

  TlsTestServerFactory tls;
  TestConnectionBasics(tls);
}

TEST(Connection, ClientReconnects) {
  constexpr uint16_t PORT = 2022; // TODO: support port randomization?

  constexpr auto MAX_ATTEMPTS = 4U;
  auto attempts = pep::MakeSharedCopy(0U);

  boost::asio::io_context io_context;
  auto client = pep::messaging::Node::Create(
    pep::networking::Tcp::ClientParameters(io_context, pep::EndPoint("localhost", PORT)),
    pep::networking::Client::ReconnectParameters(std::chrono::milliseconds(200), std::chrono::milliseconds(1000)));

  client->start()
    .subscribe(
      [client, attempts](const pep::messaging::Connection::Attempt::Result& result) {
        ASSERT_FALSE(result) << "Client connection attempt succeeded";
        if (++*attempts == MAX_ATTEMPTS) {
          client->shutdown();
        }
      },
      [client](std::exception_ptr error) {
        PEP_DEFER(client->shutdown()); // Ensure that the client is shut down even when FAIL() 
        FAIL() << "Client connectivity produced an error: " << pep::GetExceptionMessage(error);
      },
      []() { /* ignore */}
    );

  ASSERT_NO_FATAL_FAILURE(io_context.run());
  ASSERT_EQ(*attempts, MAX_ATTEMPTS) << "Client didn't make " << MAX_ATTEMPTS << " connection attempt(s)";
}
