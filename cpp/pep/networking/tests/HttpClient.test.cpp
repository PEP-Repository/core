#include <gtest/gtest.h>

#ifndef __EMSCRIPTEN__

#include <pep/async/IoContextThread.hpp>
#include <pep/httpserver/HTTPServer.hpp>
#include <pep/networking/HttpClient.hpp>
#include <pep/utils/Defer.hpp>

#include <atomic>

namespace {

using namespace std::chrono_literals;

const std::string ResponseBody = "Found someone you have, I would say, hmmm?";

class AsyncHttpServer : boost::noncopyable {
public:
  static constexpr uint16_t Port = 1880; // Port 80 might be taken by a "real" HTTP server. TODO: try random ports until we find a vacant one

private:
  std::shared_ptr<boost::asio::io_context> ioContext_ = std::make_shared<boost::asio::io_context>();
  pep::HTTPServer server_ = pep::HTTPServer(Port, ioContext_);
  pep::IoContextThread thread_ = pep::IoContextThread("HTTP server", ioContext_);

public:
  AsyncHttpServer() = default;

  ~AsyncHttpServer() noexcept {
    server_.asyncStop();
    std::this_thread::sleep_for(200ms); // Give the HTTP server time to finalize
    // thread_ destructor will (stop the associated I/O context and) block until the thread has exited
  }

  template <typename... Args>
  auto registerHandler(Args&&... args) {
    return server_.registerHandler(std::forward<Args>(args)...);
  }
};

void RegisterAndRetrieve(AsyncHttpServer& server, const std::string& relativeUri, std::shared_ptr<const pep::HTTPResponse> response) {
  ASSERT_EQ(response->getStatusCode(), 200U);

  server.registerHandler(relativeUri, false, [response](const pep::HTTPRequest&, const std::string&) {return *response; });

  boost::asio::io_context ioContext;

  auto client = pep::networking::HttpClient::Create(pep::networking::HttpClient::Parameters(ioContext, boost::urls::url("http://localhost:" + std::to_string(AsyncHttpServer::Port) + relativeUri)));
  client->start();

  auto request = client->makeRequest();
  request.setHeader("User-Agent", "Custom code"); // The neverssl Website returns a 403 if we don't specify a "User-Agent"

  auto received = pep::MakeSharedCopy(false);
  client->sendRequest(request)
    .subscribe(
      [relativeUri, received](const pep::HTTPResponse& response) {
        ASSERT_FALSE(*received) << "Received multiple responses from HTTP client";
        *received = true;
        EXPECT_EQ(2U, response.getStatusCode() / 100U) << "Got unsuccessful status code " << response.getStatusCode() << " from " << relativeUri.c_str();
        EXPECT_EQ(ResponseBody, response.getBody());
      },
      [client](const std::exception_ptr&) {client->shutdown(); },
      [client]() {client->shutdown(); }
    );

  ASSERT_NO_FATAL_FAILURE(ioContext.run());

  EXPECT_TRUE(*received) << "Didn't receive a response for HTTP request to " << relativeUri.c_str();
}

using RetryParameters = pep::networking::HttpClient::RetryParameters;

RetryParameters FastRetryParameters() {
  RetryParameters result;
  result.backoff = pep::ExponentialBackoff::Parameters(1ms, 10ms);
  result.maxDelay = 5s;
  return result;
}

/// \brief Registers a handler that produces the specified responses (repeating the last one), then sends a request and returns the response.
/// \return The response received by the client, and the number of requests received by the server.
std::pair<std::optional<pep::HTTPResponse>, unsigned> RetrieveWithRetries(AsyncHttpServer& server, const std::string& relativeUri, std::vector<pep::HTTPResponse> responses,
    RetryParameters retryParameters = FastRetryParameters(), pep::networking::HttpMethod method = pep::networking::HttpMethod::Get) {
  auto served = std::make_shared<std::atomic<unsigned>>(0U);
  server.registerHandler(relativeUri, false, [responses, served](const pep::HTTPRequest&, const std::string&) {
    auto index = (*served)++;
    return responses[std::min<size_t>(index, responses.size() - 1U)];
    });

  boost::asio::io_context ioContext;
  pep::networking::HttpClient::Parameters parameters(ioContext, boost::urls::url("http://localhost:" + std::to_string(AsyncHttpServer::Port) + relativeUri));
  parameters.retryParameters(std::move(retryParameters));
  auto client = pep::networking::HttpClient::Create(std::move(parameters));
  client->start();

  std::optional<pep::HTTPResponse> received;
  client->sendRequest(client->makeRequest(method))
    .subscribe(
      [&received](const pep::HTTPResponse& response) {
        EXPECT_FALSE(received.has_value()) << "Received multiple responses from HttpClient";
        received = response;
      },
      [client](const std::exception_ptr&) {client->shutdown(); },
      [client]() {client->shutdown(); }
    );
  ioContext.run();

  return { std::move(received), served->load() };
}

pep::HTTPResponse RetryAfterResponse(unsigned statusCode, const std::string& retryAfter) {
  return pep::HTTPResponse(statusCode, "Try again", "", pep::HTTPResponse::HeaderMap{ {"Retry-After", retryAfter} });
}

// Shares a single server between tests, since the port may not be available again (immediately) after a server has been stopped
class HttpClient : public ::testing::Test {
private:
  inline static std::unique_ptr<AsyncHttpServer> server_;

protected:
  static void SetUpTestSuite() { server_ = std::make_unique<AsyncHttpServer>(); }
  static void TearDownTestSuite() { server_.reset(); }

  AsyncHttpServer& server() { return *server_; }
};

TEST_F(HttpClient, BasicFunctioning) {
  auto& server = this->server();

  RegisterAndRetrieve(server, "/default", std::make_shared<pep::HTTPResponse>(200U, "OK", ResponseBody)); // A well-behaved response
  RegisterAndRetrieve(server, "/unsized", std::make_shared<pep::HTTPResponse>(200U, "OK", ResponseBody, pep::HTTPResponse::HeaderMap(), false)); // No "Content-Length" (or in fact any) header
  // TODO: test HTTPS as well

  // Code below has been disabled to prevent our unit test from requiring a network connection
  // RegisterAndRetrieve(boost::urls::url("https://pep.cs.ru.nl"));
}

TEST_F(HttpClient, RetriesTransientErrors) {
  auto& server = this->server();
  pep::HTTPResponse ok(200U, "OK", ResponseBody);

  // Exponential backoff
  auto [response, served] = RetrieveWithRetries(server, "/backoff", { pep::HTTPResponse(503U, "Service Unavailable"), pep::HTTPResponse(502U, "Bad Gateway"), ok });
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->getStatusCode(), 200U);
  EXPECT_EQ(response->getBody(), ResponseBody);
  EXPECT_EQ(served, 3U);

  // Retry-After in seconds, and as an HTTP-date (in the past, so we don't have to wait)
  std::tie(response, served) = RetrieveWithRetries(server, "/retry-after", { RetryAfterResponse(429U, "0"), RetryAfterResponse(503U, "Sun, 06 Nov 1994 08:49:37 GMT"), ok });
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->getStatusCode(), 200U);
  EXPECT_EQ(served, 3U);

  // Delay callback takes precedence over Retry-After
  auto parameters = FastRetryParameters();
  auto called = std::make_shared<unsigned>(0U);
  parameters.delayCallback = [called](const pep::HTTPResponse&) -> std::optional<RetryParameters::Delay> {
    ++*called;
    return 0s;
  };
  std::tie(response, served) = RetrieveWithRetries(server, "/callback", { RetryAfterResponse(429U, "3600"), ok }, parameters);
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->getStatusCode(), 200U);
  EXPECT_EQ(served, 2U);
  EXPECT_EQ(*called, 1U);
}

TEST_F(HttpClient, LimitsRetries) {
  auto& server = this->server();

  // Gives up after maxRetries, producing the last response
  auto [response, served] = RetrieveWithRetries(server, "/unavailable", { pep::HTTPResponse(503U, "Service Unavailable") });
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->getStatusCode(), 503U);
  EXPECT_EQ(served, 1U + FastRetryParameters().maxRetries);

  // Doesn't retry if the server wants us to wait longer than maxDelay
  std::tie(response, served) = RetrieveWithRetries(server, "/far-future-seconds", { RetryAfterResponse(429U, "3600") });
  EXPECT_EQ(served, 1U);
  std::tie(response, served) = RetrieveWithRetries(server, "/far-future-date", { RetryAfterResponse(503U, "Fri, 31 Dec 9999 23:59:59 GMT") });
  EXPECT_EQ(served, 1U);

  // Doesn't retry non-transient errors
  std::tie(response, served) = RetrieveWithRetries(server, "/not-found", { pep::HTTPResponse(404U, "Not Found") });
  EXPECT_EQ(served, 1U);

  // Doesn't retry POST requests if the server may have processed them...
  std::tie(response, served) = RetrieveWithRetries(server, "/post-error", { pep::HTTPResponse(500U, "Internal Server Error") }, FastRetryParameters(), pep::networking::HttpMethod::Post);
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(response->getStatusCode(), 500U);
  EXPECT_EQ(served, 1U);
  // ... but does if the server didn't
  std::tie(response, served) = RetrieveWithRetries(server, "/post-throttled", { RetryAfterResponse(429U, "0"), pep::HTTPResponse(200U, "OK", ResponseBody) }, FastRetryParameters(), pep::networking::HttpMethod::Post);
  EXPECT_EQ(served, 2U);

  // Retries can be disabled
  auto parameters = FastRetryParameters();
  parameters.maxRetries = 0;
  std::tie(response, served) = RetrieveWithRetries(server, "/disabled", { pep::HTTPResponse(503U, "Service Unavailable") }, parameters);
  EXPECT_EQ(served, 1U);
}

}

#else // (!)__EMSCRIPTEN__

namespace {
TEST(HttpClient, BasicFunctioning) {
  GTEST_SKIP() << "HttpServer not supported on Emscripten";
}
}

#endif // !__EMSCRIPTEN__
