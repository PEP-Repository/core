#include <pep/async/RxFinallyExhaust.hpp>
#include <pep/async/RxInstead.hpp>
#include <pep/async/RxIterate.hpp>
#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/messaging/Node.hpp>
#include <pep/networking/tests/TestServerFactory.test.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <boost/asio/steady_timer.hpp>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-tap.hpp>
#include <gtest/gtest.h>

using namespace std::literals;

namespace {

const auto ReadPauseDuration = 300ms;

// Handles PingRequests that have a tail. Pauses reading right away, and only lets go after a while: the tail can't arrive before that.
class PausingHandler : public pep::messaging::RequestHandler {
public:
  struct Observations {
    size_t received = 0; // Tail chunks that the handler has received
    size_t receivedWhilePaused = 0; // Idem, at the moment that the handler stops being paused
    bool completed = false;
  };

  PausingHandler(boost::asio::io_context& ioContext, std::shared_ptr<Observations> observations, bool resumeExplicitly)
    : ioContext_(ioContext), observations_(std::move(observations)), resumeExplicitly_(resumeExplicitly) {
    RegisterRequestHandlers(*this, &PausingHandler::handlePing);
  }

private:
  pep::messaging::MessageBatches handlePing(std::shared_ptr<pep::PingRequest> request, pep::messaging::MessageSequence tail, std::shared_ptr<pep::messaging::ReadThrottle> throttle) {
    EXPECT_NE(throttle, nullptr);

    // Pause reading for ReadPauseDuration.
    throttle->pause();
    auto timer = std::make_shared<boost::asio::steady_timer>(ioContext_, ReadPauseDuration);
    timer->async_wait([observations = observations_, throttle, timer, resumeExplicitly = resumeExplicitly_](const boost::system::error_code&) {
      observations->receivedWhilePaused = observations->received;
      if (resumeExplicitly) {
        throttle->resume();
      }
      // Otherwise, the (last reference to the) throttle is destroyed when this handler is destroyed, which should resume too
    });

    pep::messaging::MessageSequence response = rxcpp::observable<>::from(
      pep::MakeSharedCopy(pep::Serialization::ToString(pep::PingResponse(request->id()))));

    // Exhausts "tail" (counting its chunks along the way), then emits "response" as the single reply batch
    return tail
      .tap([observations = observations_](const std::shared_ptr<std::string>&) { ++observations->received; })
      .op(pep::RxInstead(response))
      .tap([observations = observations_](const pep::messaging::MessageSequence&) { observations->completed = true; });
  }

  boost::asio::io_context& ioContext_;
  std::shared_ptr<Observations> observations_;
  bool resumeExplicitly_;
};

[[maybe_unused]] void TestReadThrottle(bool resumeExplicitly, uint16_t port) {
  constexpr size_t ChunkCount = 5;
  constexpr uint64_t PingId = 42;

  boost::asio::io_context io_context;
  auto observations = std::make_shared<PausingHandler::Observations>();
  PausingHandler handler(io_context, observations, resumeExplicitly);

  TcpTestServerFactory factory;
  auto serverParameters = factory.createServerParameters(io_context, port);
  auto server = pep::messaging::Node::Create(*serverParameters, handler);
  auto client = pep::messaging::Node::Create(*factory.createClientParameters(*serverParameters));

  bool gotResponse = false;
  auto shutdownAll = [server, client] {
    server->shutdown().merge(client->shutdown())
      .subscribe([](pep::FakeVoid) {}, [](const std::exception_ptr&) {});
  };

  // Prevent the test from hanging (and get a proper failure) if reading never resumes
  boost::asio::steady_timer timeout(io_context, 20s);
  timeout.async_wait([&shutdownAll](const boost::system::error_code& error) {
    if (!error) {
      ADD_FAILURE() << "Timed out: reading probably never resumed";
      shutdownAll();
    }
  });

  server->start().subscribe([](const pep::messaging::Connection::Attempt::Result&) {}, [](const std::exception_ptr&) {});
  auto started = std::chrono::steady_clock::now();
  client->start().subscribe(
    [&gotResponse, shutdownAll, started, &timeout](const pep::messaging::Connection::Attempt::Result& result) {
      ASSERT_TRUE(result);
      std::vector<std::shared_ptr<std::string>> chunks;
      for (size_t i = 0; i < ChunkCount; ++i) {
        chunks.push_back(pep::MakeSharedCopy("chunk " + std::to_string(i)));
      }
      pep::messaging::MessageBatches tail = rxcpp::observable<>::just(pep::messaging::MessageSequence(pep::RxIterate(std::move(chunks)))).as_dynamic();

      (*result)->sendRequest(pep::MakeSharedCopy(pep::Serialization::ToString(pep::PingRequest(PingId))), tail)
        .subscribe(
          [&gotResponse](const std::string&) { gotResponse = true; },
          [](std::exception_ptr error) { ADD_FAILURE() << "Request failed: " << pep::GetExceptionMessage(std::move(error)); },
          [shutdownAll, started, &timeout]() {
            EXPECT_GE(std::chrono::steady_clock::now() - started, ReadPauseDuration) << "Request completed while reading should still have been paused";
            timeout.cancel();
            shutdownAll();
          });
    },
    [](std::exception_ptr error) { ADD_FAILURE() << "Client connection attempt failed: " << pep::GetExceptionMessage(std::move(error)); });

  ASSERT_NO_FATAL_FAILURE(io_context.run());

  EXPECT_EQ(observations->receivedWhilePaused, 0) << "Received messages that we should not have read while reading was paused";
  EXPECT_TRUE(observations->completed);
  EXPECT_EQ(observations->received, ChunkCount) << "Not all messages arrived after reading resumed";
  EXPECT_TRUE(gotResponse);
}

[[maybe_unused]] void TestConnectionBasics(TestServerFactory& factory) {
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
        EXPECT_EQ(connection->status(), pep::messaging::Connection::Status::Initialized) << description << " produced non-initialized connection";
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

  ASSERT_EQ(client.use_count(), 1) << "Messaging client not discardable (due to circular dependency?)";
  ASSERT_EQ(server.use_count(), 1) << "Messaging server not discardable (due to circular dependency?)";
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
  constexpr uint16_t Port = 2022; // TODO: support port randomization?

  constexpr auto MaxAttempts = 4;
  auto attempts = pep::MakeSharedCopy(0);

  boost::asio::io_context io_context;
  auto client = pep::messaging::Node::Create(
    pep::networking::Tcp::ClientParameters(io_context, pep::EndPoint("localhost", Port)),
    pep::networking::Client::ReconnectParameters(std::chrono::milliseconds(200), std::chrono::milliseconds(1000)));

  client->start()
    .subscribe(
      [client, attempts](const pep::messaging::Connection::Attempt::Result& result) {
        ASSERT_FALSE(result) << "Client connection attempt succeeded";
        if (++*attempts == MaxAttempts) {
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
  ASSERT_EQ(*attempts, MaxAttempts) << "Client didn't make " << MaxAttempts << " connection attempt(s)";

  ASSERT_EQ(client.use_count(), 1) << "Messaging client not discardable (due to circular dependency?)";
}

TEST(Connection, PausedReadingDelaysIncomingMessagesUntilResumed) {
#ifdef __EMSCRIPTEN__
  GTEST_SKIP() << "Server not supported on Emscripten";
#else
  TestReadThrottle(true, 2023);
#endif
}

TEST(Connection, DestroyingPausedThrottleResumesReading) {
#ifdef __EMSCRIPTEN__
  GTEST_SKIP() << "Server not supported on Emscripten";
#else
  TestReadThrottle(false, 2024);
#endif
}

} // End anonymous namespace
