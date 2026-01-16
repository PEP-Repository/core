#include <pep/messaging/Node.hpp>
#include <pep/networking/tests/TestServerFactory.test.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <gtest/gtest.h>

namespace {

[[maybe_unused]] void TestConnectionBasics(TestServerFactory& factory) {
  boost::asio::io_context io_context;
  pep::messaging::RequestHandler handler;

  auto protocol = factory.protocol().name();
  auto serverParameters = factory.createServerParameters(io_context, 2022); // TODO: support port randomization?
  auto server = pep::messaging::Node::Create(*serverParameters, handler);
  auto client = pep::messaging::Node::Create(*factory.createClientParameters(*serverParameters));

  server->start()
    .subscribe(
      [server, protocol](const pep::messaging::Connection::Attempt::Result& result) {
        PEP_DEFER(server->shutdown()); // Ensure that the server is shut down even if test assertions fail
        ASSERT_TRUE(result) << protocol << " server connection attempt failed";
        auto connection = *result;
        ASSERT_EQ(connection->status(), pep::messaging::Connection::Status::initialized) << protocol << " server produced non-initialized connection";
      },
      [server, protocol](std::exception_ptr error) {
        PEP_DEFER(server->shutdown()); // Ensure that the server is shut down even when FAIL()
        FAIL() << protocol << " server connectivity produced an error: " << pep::GetExceptionMessage(error);
      },
      []() { /* ignore */}
    );

  client->start()
    .subscribe(
      [client, protocol](const pep::messaging::Connection::Attempt::Result& result) {
        PEP_DEFER(client->shutdown()); // Ensure that the client is shut down even if test assertions fail
        ASSERT_TRUE(result) << protocol << " client connection attempt failed";
        auto connection = *result;
        ASSERT_EQ(connection->status(), pep::messaging::Connection::Status::initialized) << protocol << " client produced non-initialized connection";
      },
      [client, protocol](std::exception_ptr error) {
        PEP_DEFER(client->shutdown()); // Ensure that the client is shut down even when FAIL()
        FAIL() << protocol << " client connectivity produced an error: " << pep::GetExceptionMessage(error);
      },
      []() { /* ignore */}
    );

  ASSERT_NO_FATAL_FAILURE(io_context.run());
}

TEST(Connection, Basics) {
#ifdef __EMSCRIPTEN__
  GTEST_SKIP() << "Server not supported on Emscripten";
#else

  TcpTestServerFactory tcp;
  TestConnectionBasics(tcp);

  TlsTestServerFactory tls;
  TestConnectionBasics(tls);
#endif
}

TEST(Connection, ClientReconnects) {
#ifdef __EMSCRIPTEN__
  GTEST_SKIP() << "Test hangs on Emscripten with Node.js";
#else

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
#endif
}

} // End anonymous namespace
