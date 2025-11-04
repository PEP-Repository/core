#include <pep/networking/Client.hpp>
#include <pep/networking/ExponentialBackoff.hpp>
#include <pep/networking/Server.hpp>
#include <pep/networking/Tcp.hpp>
#include <pep/utils/OnFatalTestFailure.hpp>
#include <boost/algorithm/string/join.hpp>

#include <pep/utils/TestTiming.hpp>
#include <gtest/gtest.h>
#include <cmath>

using namespace std::literals;

class ClientConnectivityHandler : public std::enable_shared_from_this<ClientConnectivityHandler>, public pep::SharedConstructor<ClientConnectivityHandler> {
  friend class pep::SharedConstructor<ClientConnectivityHandler>;

private:
  pep::ExponentialBackoff::Parameters mBackoffParameters;

  std::shared_ptr<pep::networking::Client> mClient;

  pep::EventSubscription mClientStatusChangeSubscription;
  pep::EventSubscription mClientConnectionAttemptSubscription;
  bool mShutdownIssued = false;
  bool mShutdownNotified = false;

  std::shared_ptr<pep::networking::Connection> mConnection;
  pep::EventSubscription mConnectionConnectivityChangeSubscription;

  unsigned mAttempts = 0U;
  std::optional<unsigned> mSuccessfulAttempt;
  std::optional<pep::testing::TimePoint> mLastAttempt;

  unsigned unsuccessfulAttempts() const noexcept {
    auto result = mAttempts;
    if (mSuccessfulAttempt.has_value()) {
      result -= *mSuccessfulAttempt;
    }
    return result;
  }

  void handleClientStatusChange(const pep::networking::Client::StatusChange& change) {
    if (change.updated >= pep::LifeCycler::Status::finalizing) {
      ASSERT_TRUE(mShutdownIssued) << "Client sends close notification without having been shut down";
    }
    if (change.updated == pep::LifeCycler::Status::finalized) {
      ASSERT_FALSE(mShutdownNotified) << "Client sends multiple close notifications";
      mShutdownNotified = true;
    }
  }

  void handleClientConnectionAttempt(const pep::networking::Connection::Attempt::Result& result) {
    ASSERT_EQ(mConnection, nullptr) << "Client notifies connection attempt after already having connected successfully";
    if (result) {
      mConnection = *result;
      mConnectionConnectivityChangeSubscription = mConnection->onConnectivityChange.subscribe([weak = pep::WeakFrom(*this)](const pep::networking::Connection::ConnectivityChange& change) {
        pep::AcquireShared(weak)->handleConnectionConnectivityChange(change);
        });
    }
    this->handleConnectionAttempt(static_cast<bool>(result));
  }

  void handleConnectionConnectivityChange(const pep::networking::Connection::ConnectivityChange& change) {
    if (change.previous == pep::networking::Connection::ConnectivityStatus::connecting) {
      this->handleConnectionAttempt(change.updated == pep::networking::Connection::ConnectivityStatus::connected);
    }
  }

  void handleConnectionAttempt(bool successful) {
    if (successful) {
      if (mSuccessfulAttempt.has_value()) {
        PEP_DEFER(mClient->shutdown());
        FAIL() << "Unit test should only produce a single successful connection attempt; got " << *mSuccessfulAttempt << " and " << mAttempts;
      }
      mSuccessfulAttempt = mAttempts;

      // Try to read something from the (disconnected) server so that the client can (attempt to) reconnect
      auto buffer = std::make_shared<std::string>(1U, '\0');
      mConnection->asyncRead(&buffer->front(), buffer->size(), [buffer](const pep::networking::SizedTransfer::Result&) { /* ignore */});
    } else {
      this->verifyLatency(); // TODO: shut down client if test condition is failed

      if (mSuccessfulAttempt.has_value() && this->unsuccessfulAttempts() >= 3) { // Stop after a couple of failed reconnect attempts
        this->shutdownClient();
      }
    }
    // TODO: terminate at some point if no connection can be successfully established

    mLastAttempt = pep::testing::Clock::now(); // See comment in "verifyLatency" method: the client's reconnect attempt actually started a little earlier than "now"
    ++mAttempts;
  }

  void shutdownClient() {
    if (!mShutdownIssued) {
      mShutdownIssued = true;
      mClientConnectionAttemptSubscription.cancel();
      mClient->shutdown();
    }
  }

  void verifyLatency() {
    auto unsuccessful = this->unsuccessfulAttempts();
    assert(unsuccessful > 0U);

    auto latency = mBackoffParameters.minTimeout() * std::pow(mBackoffParameters.backoffFactor(), unsuccessful - 1U); // Calculate wait duration for this attempt
    latency = std::min(latency, static_cast<decltype(latency)>(mBackoffParameters.maxTimeout())); // Cap at max value
    auto latencyMsec = duration_cast<std::chrono::milliseconds>(latency); // Convert to milliseconds

    // Reconnect is started _before_ notifying us, so its timer had already been running before handleConnectionAttempt's previous invocation,
    // which assigned "now" to the "mLastAttempt" variable. Consequently, the reconnect attempt started earlier than the value that we
    // recorded, and it may therefore also currently be a little earlier than "mLastAttempt plus the expected/required latency".
    // To prevent our test from failing (as it e.g. did in https://gitlab.pep.cs.ru.nl/pep/core/-/jobs/359703#L570), we subtract some msec
    // from the (expected/required) latency. E.g. if the client (connection) needed to wait 200 msec before reconnecting, it's acceptable if it
    // re-invokes our callback after 190 msec.
    constexpr auto MAX_INVOCATION_OVERHEAD = 10ms;
    ASSERT_GE(pep::testing::MillisecondsSince(*mLastAttempt), latencyMsec - MAX_INVOCATION_OVERHEAD) << "Client didn't observe latency during reconnect attempt";
  }

  explicit ClientConnectivityHandler(const pep::ExponentialBackoff::Parameters& backoffParameters) // TODO: don't require separate reconnect parameters
    : mBackoffParameters(backoffParameters) {}

public:
  void handle(std::shared_ptr<pep::networking::Client> client) {
    assert(mClient == nullptr);

    mClient = std::move(client);
    auto weak = pep::WeakFrom(*this);

    mClientStatusChangeSubscription = mClient->onStatusChange.subscribe([weak](const pep::networking::Client::StatusChange& change) { pep::AcquireShared(weak)->handleClientStatusChange(change); });
    mClientConnectionAttemptSubscription = mClient->onConnectionAttempt.subscribe([weak](const pep::networking::Connection::Attempt::Result& result) { pep::AcquireShared(weak)->handleClientConnectionAttempt(result); });
  }

  void postRunValidate() {
    ASSERT_TRUE(mShutdownNotified) << "Client didn't send shutdown notification";
    ASSERT_TRUE(mLastAttempt.has_value()) << "Client didn't attempt to connect to server";
    ASSERT_GT(mAttempts, 0U) << "Client didn't attempt to connect to server";

    auto minBackoffMsec = duration_cast<std::chrono::milliseconds>(mBackoffParameters.minTimeout());
    ASSERT_LT(pep::testing::MillisecondsSince(*mLastAttempt), minBackoffMsec) << "Client shutdown (and hence I/O context termination) shouldn't wait for exponential backoff. Does the Client cancel the timer?";

    ASSERT_GT(mAttempts, 1U) << "Client should have made at least two connection attempts: one successful plus one unsuccessful";
    ASSERT_TRUE(mSuccessfulAttempt.has_value()) << "Client couldn't connect to server";
    ASSERT_GT(this->unsuccessfulAttempts(), 0U) << "Client didn't attempt to reconnect after connection was lost";
  }
};

TEST(Client, Reconnects) { // TODO: simplify
  boost::asio::io_context ioContext;

  auto server = pep::networking::Server::Create(pep::networking::Tcp::ServerParameters(ioContext, pep::networking::Tcp::ServerParameters::RANDOM_PORT));
  auto serverConnectionAttempt = std::make_shared<pep::EventSubscription>();
  *serverConnectionAttempt = server->onConnectionAttempt.subscribe([server, serverConnectionAttempt](const pep::networking::Connection::Attempt::Result& result) {
    serverConnectionAttempt->cancel(); // Break circular reference
    server->shutdown(); // Ensure that we don't keep scheduling work on the I/O context, even if the assertion fails
    ASSERT_TRUE(result) << "Server connection failed"; // Ensure that the client has at least one successful connection attempt
    });
  server->start();

  auto backoffParams = pep::ExponentialBackoff::Parameters(200ms, 1s);
  auto clientParameters = server->createClientParameters();
  auto client = pep::networking::Client::Create(*clientParameters, backoffParams);

  auto clientHandler = ClientConnectivityHandler::Create(backoffParams);
  clientHandler->handle(client);

  client->start();

  ASSERT_NO_FATAL_FAILURE(ioContext.run());
  ASSERT_NO_FATAL_FAILURE(clientHandler->postRunValidate());
}

namespace {

const std::vector<std::string> LINES_TO_DELIMIT = {
  "The clock struck one, the mouse ran down.",
  "The clock struck two, the mouse went WOO.",
  "The clock struck three, the mouse went WEEEEEE.",
  "The clock struck four, the mouse said 'NO MORE'."
};
const char* const LINE_DELIMITER = "\r\n";
const std::string DELIMITED_CONTENT = boost::algorithm::join(LINES_TO_DELIMIT, LINE_DELIMITER);

void ReadClientLine(std::shared_ptr<pep::networking::Client> client, std::shared_ptr<pep::networking::Connection> connection, unsigned index) {
  connection->asyncReadUntil(LINE_DELIMITER, [client, connection, index](const pep::networking::DelimitedTransfer::Result& result) {
    if (!result) {
      client->shutdown(); // Ensure that we don't keep scheduling work on the I/O context even if the assertion fails
      // Last line doesn't have a trailing delimiter, so the read is expected to fail for that index
      ASSERT_EQ(index, LINES_TO_DELIMIT.size() - 1U) << "Receiving non-last client line " << index << " failed";
    }
    else {
      EXPECT_EQ(*result, LINES_TO_DELIMIT[index] + LINE_DELIMITER) << "Delimited read didn't receive expected data";
      auto next = index + 1U;
      if (next < LINES_TO_DELIMIT.size()) {
        ReadClientLine(client, connection, next);
      }
      else {
        client->shutdown(); // Stop scheduling work on the I/O context
      }
    }
    });
}

} // End anonymous namespace

TEST(Client, ReadUntil) {
  boost::asio::io_context ioContext;

  auto server = pep::networking::Server::Create(pep::networking::Tcp::ServerParameters(ioContext, pep::networking::Tcp::ServerParameters::RANDOM_PORT));
  auto serverConnectionAttempt = std::make_shared<pep::EventSubscription>();
  *serverConnectionAttempt = server->onConnectionAttempt.subscribe([server, serverConnectionAttempt](const pep::networking::Connection::Attempt::Result& result) {
    PEP_ON_FATAL_TEST_FAILURE(serverConnectionAttempt->cancel();  server->shutdown());
    ASSERT_TRUE(result) << "Server connection failed";

    (*result)->asyncWrite(DELIMITED_CONTENT.c_str(), DELIMITED_CONTENT.size(), [server, serverConnectionAttempt](const pep::networking::SizedTransfer::Result& result) {
      serverConnectionAttempt->cancel();
      server->shutdown(); // Ensure that we don't keep scheduling work on the I/O context, even if the assertion fails
      ASSERT_TRUE(result) << "Sending server content failed";
      });
    });
  server->start();

  auto clientConnectionAttempt = std::make_shared<pep::EventSubscription>();
  auto client = pep::networking::Client::Create(*server->createClientParameters());
  *clientConnectionAttempt = client->onConnectionAttempt.subscribe([client](const pep::networking::Connection::Attempt::Result& result) {
    if (result && (*result)->isConnected()) {
      ReadClientLine(client, *result, 0U);
    }
    });
  client->start();

  ASSERT_NO_FATAL_FAILURE(ioContext.run());
}

TEST(Client, ReadAll) {
  boost::asio::io_context ioContext;

  auto server = pep::networking::Server::Create(pep::networking::Tcp::ServerParameters(ioContext, pep::networking::Tcp::ServerParameters::RANDOM_PORT));
  auto serverConnectionAttemptSub = std::make_shared<pep::EventSubscription>();
  *serverConnectionAttemptSub = server->onConnectionAttempt.subscribe([server, serverConnectionAttemptSub](const pep::networking::Connection::Attempt::Result& result) {
    PEP_ON_FATAL_TEST_FAILURE(serverConnectionAttemptSub->cancel();  server->shutdown());
    ASSERT_TRUE(result) << "Server connection failed";

    (*result)->asyncWrite("woohoo", 6U, [server, serverConnectionAttemptSub](const pep::networking::SizedTransfer::Result& result) {
      serverConnectionAttemptSub->cancel();
      server->shutdown(); // Ensure that we don't keep scheduling work on the I/O context, even if the assertion fails
      ASSERT_TRUE(result) << "Sending server content failed";
      });
    });
  server->start();

  auto client = pep::networking::Client::Create(*server->createClientParameters());
  auto clientConnectionAttemptSub = client->onConnectionAttempt.subscribe([client](const pep::networking::Connection::Attempt::Result& result) {
    ASSERT_TRUE(result);
    auto connection = *result;

    // The connection will close (and try to reconnect) automatically after our call to "asyncReadAll"
    auto connectivityChangeSub = std::make_shared<pep::EventSubscription>();
    *connectivityChangeSub = connection->onConnectivityChange.subscribe([connectivityChangeSub, client](const pep::networking::Connection::ConnectivityChange& change) {
      PEP_DEFER(client->shutdown()); // Stop trying to reconnect
      connectivityChangeSub->cancel(); // Break circular reference
      ASSERT_NE(change.updated, pep::networking::Connection::ConnectivityStatus::connected);
      });

    connection->asyncReadAll([client, connection](const pep::networking::DelimitedTransfer::Result& result) {
      ASSERT_TRUE(result);
      EXPECT_EQ(*result, "woohoo");
      ASSERT_TRUE(connection->isConnected()) << "Existing socket (and hence connection) should still be considered 'connected' when result is produced";
      });
    });
  client->start();

  ASSERT_NO_FATAL_FAILURE(ioContext.run());
}
