#include <pep/networking/Client.hpp>
#include <pep/networking/ExponentialBackoff.hpp>
#include <pep/networking/Server.hpp>
#include <pep/networking/Tcp.hpp>
#include <pep/utils/OnFatalTestFailure.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <pep/utils/TestTiming.hpp>
#include <pep/networking/Connection.hpp>
#include <pep/networking/Transport.hpp> // for networking::SizedTransfer, networking::DelimitedTransfer
#include <pep/utils/Defer.hpp>
#include <pep/utils/Event.hpp>
#include <pep/utils/LifeCycler.hpp>
#include <pep/utils/Shared.hpp>
#include <gtest/gtest.h>
#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace std::literals;

namespace {

class Client : public ::testing::Test {
public:
  static void SetUpTestSuite() {
#ifdef __EMSCRIPTEN__
    GTEST_SKIP() << "Server not supported on Emscripten";
#endif
  }
};

/// \brief Verifies that a client reconnects (observing exponential backoff) after losing its connection, and that shutting the
/// client down cancels a pending reconnect attempt.
/// \details Timing is only verified in ways that a busy/slow machine can't break:
/// - the client's backoff between reconnect attempts must be _at least_ the expected duration, and
/// - the I/O context must terminate _before_ a (pending) reconnect attempt would have been made. We shut the client down
///   while it waits for such an attempt, using backoff parameters that make that wait very long compared to the rest of
///   the test.
class ClientConnectivityHandler : public std::enable_shared_from_this<ClientConnectivityHandler>, public pep::SharedConstructor<ClientConnectivityHandler> {
  friend class pep::SharedConstructor<ClientConnectivityHandler>;

public:
  /// The number of failed reconnect attempts after which the client is shut down
  static constexpr unsigned RequiredReconnectFailures = 2U;

private:
  boost::asio::io_context& ioContext_;
  pep::ExponentialBackoff::Parameters backoffParameters_;

  std::shared_ptr<pep::networking::Client> client_;

  pep::EventSubscription clientStatusChangeSubscription_;
  pep::EventSubscription clientConnectionAttemptSubscription_;
  bool shutdownIssued_ = false;
  bool shutdownNotified_ = false;

  std::shared_ptr<pep::networking::Connection> connection_;
  pep::EventSubscription connectionConnectivityChangeSubscription_;

  unsigned attempts_ = 0U;
  /// The (1-based) index of the successful attempt, if one occurred.
  std::optional<unsigned> successfulAttempt_;
  std::optional<pep::testing::TimePoint> lastAttempt_;

  /// The number of (failed) attempts after the successful one
  unsigned unsuccessfulAttempts() const noexcept {
    auto result = attempts_;
    if (successfulAttempt_.has_value()) {
      result -= *successfulAttempt_;
    }
    return result;
  }

  void handleClientStatusChange(const pep::networking::Client::StatusChange& change) {
    if (change.updated >= pep::LifeCycler::Status::Finalizing) {
      ASSERT_TRUE(shutdownIssued_) << "Client sends close notification without having been shut down";
    }
    if (change.updated == pep::LifeCycler::Status::Finalized) {
      ASSERT_FALSE(shutdownNotified_) << "Client sends multiple close notifications";
      shutdownNotified_ = true;
    }
  }

  void handleClientConnectionAttempt(const pep::networking::Connection::Attempt::Result& result) {
    ASSERT_EQ(connection_, nullptr) << "Client notifies connection attempt after already having connected successfully";
    if (result) {
      connection_ = *result;
      connectionConnectivityChangeSubscription_ = connection_->onConnectivityChange.subscribe([weak = pep::WeakFrom(*this)](const pep::networking::Connection::ConnectivityChange& change) {
        pep::AcquireShared(weak)->handleConnectionConnectivityChange(change);
        });
    }
    this->handleConnectionAttempt(static_cast<bool>(result));
  }

  void handleConnectionConnectivityChange(const pep::networking::Connection::ConnectivityChange& change) {
    if (change.previous == pep::networking::Connection::ConnectivityStatus::Connecting) {
      this->handleConnectionAttempt(change.updated == pep::networking::Connection::ConnectivityStatus::Connected);
    }
  }

  void handleConnectionAttempt(bool successful) {
    ++attempts_;
    if (successful) {
      if (successfulAttempt_.has_value()) {
        PEP_DEFER(client_->shutdown());
        FAIL() << "Unit test should only produce a single successful connection attempt; got " << *successfulAttempt_ << " and " << attempts_;
      }
      successfulAttempt_ = attempts_;

      // Try to read something from the (disconnected) server so that the client can (attempt to) reconnect
      auto buffer = std::make_shared<std::string>(1U, '\0');
      connection_->asyncRead(&buffer->front(), buffer->size(), [buffer](const pep::networking::SizedTransfer::Result&) { /* ignore */});
    } else if (successfulAttempt_.has_value()) { // A reconnect attempt
      // (We don't verify latency of initial connection attempts: the client notifies those _after_ it starts waiting.)
      this->verifyLatency();

      if (this->unsuccessfulAttempts() == RequiredReconnectFailures) {
        // Shut down the client while it waits for its next reconnect attempt. Since the client starts waiting _after_
        // notifying us of this (failed) attempt, we can't shut it down right away: we'd prevent it from waiting at all.
        boost::asio::post(ioContext_, [weak = pep::WeakFrom(*this)] { pep::AcquireShared(weak)->shutdownClient(); });
      }
    }
    // TODO: terminate at some point if no connection can be successfully established

    // The client notifies us synchronously, and only starts waiting for its next (re)connect attempt after we return.
    // So the wait starts _after_ this point in time: see "verifyLatency" and "postRunValidate".
    lastAttempt_ = pep::testing::Clock::now();
  }

  void shutdownClient() {
    if (!shutdownIssued_) {
      shutdownIssued_ = true;
      clientConnectionAttemptSubscription_.cancel();
      client_->shutdown();
    }
  }

  void verifyLatency() {
    auto unsuccessful = this->unsuccessfulAttempts();
    assert(unsuccessful > 0U);

    // The client started waiting for this attempt after (losing the connection that it established in) the previous
    // attempt, i.e. after we recorded "lastAttempt_". So at least the backoff duration must have passed since then. This
    // holds regardless of how much longer the client (or this callback) may have been delayed by a busy machine.
    auto backoff = backoffParameters_.timeoutAfter(unsuccessful - 1U);
    ASSERT_GE(pep::testing::Clock::now() - *lastAttempt_, backoff) << "Client didn't observe latency during reconnect attempt";
  }

  ClientConnectivityHandler(boost::asio::io_context& ioContext, const pep::ExponentialBackoff::Parameters& backoffParameters) // TODO: don't require separate reconnect parameters
    : ioContext_(ioContext), backoffParameters_(backoffParameters) {}

public:
  void handle(std::shared_ptr<pep::networking::Client> client) {
    assert(client_ == nullptr);

    client_ = std::move(client);
    auto weak = pep::WeakFrom(*this);

    clientStatusChangeSubscription_ = client_->onStatusChange.subscribe([weak](const pep::networking::Client::StatusChange& change) { pep::AcquireShared(weak)->handleClientStatusChange(change); });
    clientConnectionAttemptSubscription_ = client_->onConnectionAttempt.subscribe([weak](const pep::networking::Connection::Attempt::Result& result) { pep::AcquireShared(weak)->handleClientConnectionAttempt(result); });
  }

  void postRunValidate() {
    ASSERT_TRUE(shutdownNotified_) << "Client didn't send shutdown notification";
    ASSERT_TRUE(lastAttempt_.has_value()) << "Client didn't attempt to connect to server";
    ASSERT_GT(attempts_, 0U) << "Client didn't attempt to connect to server";
    ASSERT_TRUE(successfulAttempt_.has_value()) << "Client couldn't connect to server";
    ASSERT_EQ(this->unsuccessfulAttempts(), RequiredReconnectFailures)
      << "Client didn't attempt to reconnect (often enough) after connection was lost";

    // The client was shut down while waiting for its next reconnect attempt, which it started after we recorded
    // "lastAttempt_". If shutdown didn't cancel that wait, the I/O context would have kept running until the wait was
    // over, i.e. until at least the backoff duration had passed since "lastAttempt_".
    auto pendingBackoff = backoffParameters_.timeoutAfter(this->unsuccessfulAttempts());
    ASSERT_LT(pep::testing::Clock::now() - *lastAttempt_, pendingBackoff)
      << "Client shutdown (and hence I/O context termination) shouldn't wait for exponential backoff. Does the Client cancel the timer?";
  }
};

TEST_F(Client, Reconnects) { // TODO: simplify

  boost::asio::io_context ioContext;

  auto server = pep::networking::Server::Create(pep::networking::Tcp::ServerParameters(ioContext, pep::networking::Tcp::ServerParameters::RandomPort));
  auto serverConnectionAttempt = std::make_shared<pep::EventSubscription>();
  *serverConnectionAttempt = server->onConnectionAttempt.subscribe([server, serverConnectionAttempt](const pep::networking::Connection::Attempt::Result& result) {
    serverConnectionAttempt->cancel(); // Break circular reference
    server->shutdown(); // Ensure that we don't keep scheduling work on the I/O context, even if the assertion fails
    ASSERT_TRUE(result) << "Server connection failed"; // Ensure that the client has at least one successful connection attempt
    });
  server->start();

  // The client waits 25ms and 1s for its (failing) reconnect attempts. Then it's shut down while waiting 40s (25ms*(40^2)) for the
  // next attempt: the test fails if the I/O context keeps running that long.
  static_assert(ClientConnectivityHandler::RequiredReconnectFailures == 2U, "Backoff parameters are tailored to this number of reconnect failures");
  auto backoffParams = pep::ExponentialBackoff::Parameters(25ms, 1min, 40);
  auto clientParameters = server->createClientParameters();
  auto client = pep::networking::Client::Create(*clientParameters, backoffParams);

  auto clientHandler = ClientConnectivityHandler::Create(ioContext, backoffParams);
  clientHandler->handle(client);

  client->start();

  ASSERT_NO_FATAL_FAILURE(ioContext.run());
  ASSERT_NO_FATAL_FAILURE(clientHandler->postRunValidate());
}


const std::vector<std::string> LinesToDelimit = {
  "The clock struck one, the mouse ran down.",
  "The clock struck two, the mouse went WOO.",
  "The clock struck three, the mouse went WEEEEEE.",
  "The clock struck four, the mouse said 'NO MORE'."
};
const char* const LINE_DELIMITER = "\r\n";
const std::string DelimitedContent = boost::algorithm::join(LinesToDelimit, LINE_DELIMITER);

void ReadClientLine(std::shared_ptr<pep::networking::Client> client, std::shared_ptr<pep::networking::Connection> connection, unsigned index) {
  connection->asyncReadUntil(LINE_DELIMITER, [client, connection, index](const pep::networking::DelimitedTransfer::Result& result) {
    if (!result) {
      client->shutdown(); // Ensure that we don't keep scheduling work on the I/O context even if the assertion fails
      // Last line doesn't have a trailing delimiter, so the read is expected to fail for that index
      ASSERT_EQ(index, LinesToDelimit.size() - 1U) << "Receiving non-last client line " << index << " failed";
    }
    else {
      EXPECT_EQ(*result, LinesToDelimit[index] + LINE_DELIMITER) << "Delimited read didn't receive expected data";
      auto next = index + 1U;
      if (next < LinesToDelimit.size()) {
        ReadClientLine(client, connection, next);
      }
      else {
        client->shutdown(); // Stop scheduling work on the I/O context
      }
    }
    });
}


TEST_F(Client, ReadUntil) {
  boost::asio::io_context ioContext;

  auto server = pep::networking::Server::Create(pep::networking::Tcp::ServerParameters(ioContext, pep::networking::Tcp::ServerParameters::RandomPort));
  auto serverConnectionAttempt = std::make_shared<pep::EventSubscription>();
  *serverConnectionAttempt = server->onConnectionAttempt.subscribe([server, serverConnectionAttempt](const pep::networking::Connection::Attempt::Result& result) {
    PEP_ON_FATAL_TEST_FAILURE(serverConnectionAttempt->cancel();  server->shutdown());
    ASSERT_TRUE(result) << "Server connection failed";

    (*result)->asyncWrite(DelimitedContent.c_str(), DelimitedContent.size(), [server, serverConnectionAttempt](const pep::networking::SizedTransfer::Result& result) {
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

TEST_F(Client, ReadAll) {
  boost::asio::io_context ioContext;

  auto server = pep::networking::Server::Create(pep::networking::Tcp::ServerParameters(ioContext, pep::networking::Tcp::ServerParameters::RandomPort));
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
      ASSERT_NE(change.updated, pep::networking::Connection::ConnectivityStatus::Connected);
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

}
