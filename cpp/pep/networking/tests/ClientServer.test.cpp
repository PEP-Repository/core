#include <pep/networking/tests/TestServerFactory.test.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Random.hpp>

#include <gtest/gtest.h>

namespace {

void TestClientServerBasics(TestServerFactory& factory) {
  const size_t MESSAGE_SIZE = 1024;

  boost::asio::io_context context;

  auto sent = std::make_shared<std::string>(), received = std::make_shared<std::string>();
  pep::RandomBytes(*sent, MESSAGE_SIZE);
  received->resize(MESSAGE_SIZE);

  auto protocol = factory.protocolName();

  auto server = factory.createServer(context, pep::networking::TcpBasedProtocol::ServerParameters::RANDOM_PORT);
  auto started = pep::MakeSharedCopy(false), stopped = pep::MakeSharedCopy(false);
  auto serverConnectionAttempt = std::make_shared<pep::EventSubscription>();
  *serverConnectionAttempt = server->onConnectionAttempt.subscribe([MESSAGE_SIZE, sent, server, serverConnectionAttempt, started, stopped, protocol](const pep::networking::Connection::Attempt::Result& result) {
    ASSERT_FALSE(*started) << protocol << " server produced multiple ConnectResults";
    *started = true;

    EXPECT_TRUE(result) << protocol << " server connection failed: " << pep::GetExceptionMessage(result.exception());
    if (!result) {
      ASSERT_FALSE(*stopped) << protocol << " server cannot be stopped multiple times";
      serverConnectionAttempt->cancel();
      server->shutdown();
      *stopped = true;
      return;
    }

    auto connection = *result;
    ASSERT_TRUE(connection != nullptr) << protocol << " server produced NULL connection";
    ASSERT_TRUE(connection->isConnected()) << protocol << " server produced non-connected connection";
    ASSERT_FALSE(*stopped) << protocol << " server cannot be stopped multiple times";

    connection->asyncWrite(sent->data(), MESSAGE_SIZE, [MESSAGE_SIZE, server, serverConnectionAttempt, stopped, protocol, connection](const pep::networking::SizedTransfer::Result& result) {
      ASSERT_FALSE(*stopped) << protocol << " server cannot be stopped multiple times";
      // Ensure that our server stops (and hence the process exits) even if a test assertion (below) fails
      serverConnectionAttempt->cancel();
      server->shutdown();
      *stopped = true;

      ASSERT_TRUE(result) << protocol << " async write produced an error: " << pep::GetExceptionMessage(result.exception());
      ASSERT_EQ(MESSAGE_SIZE, *result) << protocol << " async write didn't write expected number of bytes";
      });
    });
  server->start();

  auto client = factory.createClient();
  auto connected = pep::MakeSharedCopy(false);
  auto clientConnectionAttempt = std::make_shared<pep::EventSubscription>();
  *clientConnectionAttempt = client->onConnectionAttempt.subscribe([MESSAGE_SIZE, &client, clientConnectionAttempt, received, connected, protocol](const pep::networking::Connection::Attempt::Result& result) {
    ASSERT_TRUE(result) << protocol << " client connection failed: " << pep::GetExceptionMessage(result.exception());
    auto connection = *result;
    ASSERT_TRUE(connection != nullptr) << protocol << " client produced NULL connection";
    *connected = connection->isConnected();
    ASSERT_TRUE(*connected) << protocol << " client produced non-connected connection";

    connection->asyncRead(received->data(), MESSAGE_SIZE, [MESSAGE_SIZE, &client, clientConnectionAttempt, protocol](const pep::networking::SizedTransfer::Result& result) {
      // Ensure that the client is discarded (and hence the process exits) even if a test assertion (below) fails
      clientConnectionAttempt->cancel();
      client.reset();

      ASSERT_TRUE(result) << protocol << " async read produced an error: " << pep::GetExceptionMessage(result.exception());
      ASSERT_EQ(MESSAGE_SIZE, *result) << protocol << " async read didn't write expected number of bytes";
      });
    });
  client->start();

  ASSERT_NO_THROW(context.run());

  ASSERT_TRUE(*started) << protocol << " server didn't produce a ConnectResult";
  ASSERT_TRUE(*stopped) << protocol << " server didn't stop";
  ASSERT_TRUE(*connected) << protocol << " client didn't connect";
  ASSERT_EQ(*sent, *received) << protocol << " data was corrupted during transfer";
}

}

TEST(ClientServer, Tcp) {
  TcpTestServerFactory factory;
  TestClientServerBasics(factory);
}

TEST(ClientServer, Tls) {
  TlsTestServerFactory factory;
  TestClientServerBasics(factory);
}
