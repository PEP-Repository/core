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

  auto runNode = [&io_context, protocol](std::shared_ptr<pep::messaging::Node> node, const std::string& type, std::shared_ptr<pep::messaging::Node> other) {
    auto description = protocol + ' ' + type;
    node->start()
      .concat_map([node, description](const pep::messaging::Connection::Attempt::Result& result) {
      EXPECT_TRUE(result) << description << " connection attempt failed";
      if (result) {
        auto connection = *result;
        EXPECT_EQ(connection->status(), pep::messaging::Connection::Status::initialized) << description << " produced non-initialized connection";
      }
      return node->shutdown();
        })
      .op(pep::RxFinallyExhaust(io_context, [other]() { return other->shutdown(); })) // Ensure that the test terminates even if the other party doesn't signal a connection attempt, e.g. because its (TLS) connection is never fully established
      .subscribe(
        [](pep::FakeVoid) { /* ignore */ },
        [description](std::exception_ptr error) {
          FAIL() << description << " connectivity produced an error : " << pep::GetExceptionMessage(error);
        },
        []() { /* ignore */}
      );
    };

  runNode(server, "server", client);
  runNode(client, "client", server);

  ASSERT_NO_FATAL_FAILURE(io_context.run());

  ASSERT_EQ(client.use_count(), 1U) << "Messaging client not discardable (due to circular dependency?)";
  ASSERT_EQ(server.use_count(), 1U) << "Messaging server not discardable (due to circular dependency?)";
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

  ASSERT_EQ(client.use_count(), 1U) << "Messaging client not discardable (due to circular dependency?)";
}
