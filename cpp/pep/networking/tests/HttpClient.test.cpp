#include <gtest/gtest.h>

#ifndef __EMSCRIPTEN__

#include <pep/async/IoContextThread.hpp>
#include <pep/httpserver/HTTPServer.hpp>
#include <pep/networking/HttpClient.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <atomic>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace {

using namespace std::chrono_literals;

const std::string ResponseBody = "Found someone you have, I would say, hmmm?";
const std::string LogTag = "HttpClientTest";


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

/// \brief Minimal HTTP server that keeps connections open between requests (unlike our civetweb-based HTTPServer).
/// \details It responds to every request with the request's path as the body, and records the requests that it received completely.
/// Requests for PausingPath are paused after their header has been received: the onPausingRequest callback is invoked,
/// and the request's body is read (and the response is sent) when the resumeFunction passed to the callback is invoked.
class KeepAliveHttpServer : boost::noncopyable {
public:
  static inline const std::string PausingPath = "/pause";
  using resumeFunction=std::function<void()>;
  std::function<void(const resumeFunction&)> onPausingRequest;

  struct ReceivedRequest {
    std::string path;
    std::string body;
  };

private:
  std::vector<ReceivedRequest> receivedRequests_;

  class Session : public std::enable_shared_from_this<Session> {
  private:
    KeepAliveHttpServer& server_;
    boost::asio::streambuf buffer_;

  public:
    boost::asio::ip::tcp::socket socket;
    boost::asio::steady_timer timer;

    Session(KeepAliveHttpServer& server, boost::asio::ip::tcp::socket socket)
      : server_(server), socket(std::move(socket)), timer(this->socket.get_executor()) {
    }

    void readRequest() {
      boost::asio::async_read_until(socket, buffer_, "\r\n\r\n", [self = shared_from_this()](const boost::system::error_code& error, size_t size) {
        if (error) {
          PEP_LOG(LogTag, pep::Severity::Debug) << "Failed to read request: " << error.message();
          return;
        }
        auto begin = boost::asio::buffers_begin(self->buffer_.data());
        std::istringstream header(std::string(begin, begin + static_cast<std::ptrdiff_t>(size)));
        self->buffer_.consume(size);

        std::string method, path;
        header >> method >> path;
        auto contentLength = ParseContentLength(header);
        if (path == PausingPath) {
          if (self->server_.onPausingRequest) {
            self->server_.onPausingRequest([self, path, contentLength]() {
              self->readBody(path, contentLength);
            });
          }
        }
        else {
          self->readBody(path, contentLength);
        }
        });
    }

    static size_t ParseContentLength(std::istream& header) {
      std::string line;
      while (std::getline(header, line)) {
        auto colon = line.find(':');
        if (colon != std::string::npos && boost::iequals(line.substr(0, colon), "Content-Length")) {
          return std::stoull(line.substr(colon + 1));
        }
      }
      return 0;
    }

    void readBody(const std::string& path, size_t contentLength) {
      if (buffer_.size() >= contentLength) {
        this->receiveRequest(path, contentLength);
        return;
      }
      boost::asio::async_read(socket, buffer_, boost::asio::transfer_exactly(contentLength - buffer_.size()),
        [self = shared_from_this(), path, contentLength](const boost::system::error_code& error, size_t) {
          if (error) {
            PEP_LOG(LogTag, pep::Severity::Debug) << "Failed to read request body: " << error.message();
            return;
          }
          self->receiveRequest(path, contentLength);
        });
    }

    void receiveRequest(const std::string& path, size_t contentLength) {
      auto begin = boost::asio::buffers_begin(buffer_.data());
      server_.receivedRequests_.push_back({ path, std::string(begin, begin + static_cast<std::ptrdiff_t>(contentLength)) });
      buffer_.consume(contentLength);
      this->respond(path);
    }

    void respond(const std::string& body) {
      auto response = std::make_shared<std::string>("HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body);
      boost::asio::async_write(socket, boost::asio::buffer(*response), [self = shared_from_this(), response](const boost::system::error_code& error, size_t) {
        if (error) {
          PEP_LOG(LogTag, pep::Severity::Debug) << "Failed to write response: " << error.message();
          return;
        }
        self->readRequest();
        });
    }
  };

  boost::asio::ip::tcp::acceptor acceptor_;
  std::vector<std::shared_ptr<Session>> sessions_;

  void accept() {
    acceptor_.async_accept([this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
      if (error) {
        PEP_LOG(LogTag, pep::Severity::Debug) << "Failed to accept connection: " << error.message();
        return;
      }
      auto session = std::make_shared<Session>(*this, std::move(socket));
      sessions_.push_back(session);
      session->readRequest();
      this->accept();
      });
  }

public:
  explicit KeepAliveHttpServer(boost::asio::io_context& ioContext)
    : acceptor_(ioContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0)) {
    this->accept();
  }

  uint16_t port() const { return acceptor_.local_endpoint().port(); }
  const std::vector<ReceivedRequest>& receivedRequests() const { return receivedRequests_; }

  void stop() {
    boost::system::error_code ignored;
    (void) acceptor_.close(ignored);
    for (const auto& session : sessions_) {
      session->timer.cancel();
      (void) session->socket.close(ignored);
    }
    sessions_.clear();
  }
};

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

// Not part of the HttpClient fixture: this test uses its own server
TEST(HttpClientConnection, AbandonedRequestDoesNotAffectSubsequentRequests) {
  boost::asio::io_context ioContext;
  KeepAliveHttpServer server(ioContext);

  auto client = pep::networking::HttpClient::Create(pep::networking::HttpClient::Parameters(ioContext, boost::urls::url("http://127.0.0.1:" + std::to_string(server.port()))));
  client->start();

  const std::vector<std::string> followupPaths = { "/first", "/second", "/third" };
  auto receivedCount = std::make_shared<size_t>(0);

  auto finish = [client, &server]() {
    client->shutdown();
    server.stop();
  };

  auto abandoned = client->sendRequest(client->makeRequest(pep::networking::HttpMethod::Get, KeepAliveHttpServer::PausingPath))
    .subscribe(
      [](const pep::HTTPResponse&) { ADD_FAILURE() << "Received response for abandoned request"; },
      [](const std::exception_ptr&) { ADD_FAILURE() << "Received error for abandoned request"; }
    );

  // When the server has received the pausing request, the client is waiting for its response: abandon the request and send new ones.
  // Previously the client then sent new requests over the same connection, and read the abandoned request's response for them.
  server.onPausingRequest = [&](const KeepAliveHttpServer::resumeFunction& resume) {
    abandoned.unsubscribe();
    resume();
    for (const auto& path : followupPaths) {
      client->sendRequest(client->makeRequest(pep::networking::HttpMethod::Get, path))
        .subscribe(
          [path, receivedCount, &followupPaths, &finish](const pep::HTTPResponse& response) {
            EXPECT_EQ(200U, response.getStatusCode()) << "for request to " << path;
            EXPECT_EQ(path, response.getBody()) << "Received response for another request";
            if (++*receivedCount == followupPaths.size()) {
              finish();
            }
          },
          [path](const std::exception_ptr& error) { ADD_FAILURE() << "Request to " << path << " failed: " << pep::GetExceptionMessage(error); }
        );
    }
  };

  ioContext.run_for(30s);
  EXPECT_EQ(followupPaths.size(), *receivedCount) << "Not all requests sent after the abandoned one received a response";

  if (*receivedCount != followupPaths.size()) { // Timed out: clean up, and let pending handlers finish
    finish();
    ioContext.restart();
    ioContext.run_for(30s);
  }
}

TEST(HttpClientConnection, RequestAbandonedWhileSendingIsNotInterleaved) {
  boost::asio::io_context ioContext;
  KeepAliveHttpServer server(ioContext);

  auto client = pep::networking::HttpClient::Create(pep::networking::HttpClient::Parameters(ioContext, boost::urls::url("http://127.0.0.1:" + std::to_string(server.port()))));
  client->start();
  const std::vector<std::string> followupPaths = { "/first", "/second", "/third" };
  auto receivedCount = std::make_shared<size_t>(0);

  auto makePut = [client](const std::string& path) {
    auto request = client->makeRequest(pep::networking::HttpMethod::Put, path);
    request.getBodyparts() = {std::make_shared<std::string>("Request for "), std::make_shared<std::string>(path)};
    return request;
  };

  auto finish = [client, &server]() {
    client->shutdown();
    server.stop();
  };

  auto abandoned = client->sendRequest(makePut(KeepAliveHttpServer::PausingPath))
    .subscribe(
      [](const pep::HTTPResponse&) { ADD_FAILURE() << "Received response for abandoned request"; },
      [](const std::exception_ptr&) { ADD_FAILURE() << "Received error for abandoned request"; }
    );

  // When the server has received the pausing request's header, the client is still sending its body: abandon the request and send new ones.
  // Previously the client then sent the new requests over the same connection, causing the server to receive them as part of the abandoned request's body.
  server.onPausingRequest = [&](const KeepAliveHttpServer::resumeFunction& resume) {
    abandoned.unsubscribe();
    resume();
    for (const auto& path : followupPaths) {
      client->sendRequest(makePut(path))
        .subscribe(
          [path, receivedCount, &followupPaths, &finish](const pep::HTTPResponse& response) {
            EXPECT_EQ(200U, response.getStatusCode()) << "for request to " << path;
            EXPECT_EQ(path, response.getBody()) << "Received response for another request";
            if (++*receivedCount == followupPaths.size()) {
              finish();
            }
          },
          [path](const std::exception_ptr& error) { ADD_FAILURE() << "Request to " << path << " failed: " << pep::GetExceptionMessage(error); }
        );
    }
  };

  ioContext.run_for(30s);
  EXPECT_EQ(followupPaths.size(), *receivedCount) << "Not all requests sent after the abandoned one received a response";

  if (*receivedCount != followupPaths.size()) { // Timed out: clean up, and let pending handlers finish
    finish();
    ioContext.restart();
    ioContext.run_for(30s);
  }

  // The server may or may not have received the abandoned request (completely), but every request it did receive must be exactly what the client sent
  for (const auto& received : server.receivedRequests()) {
    if (received.path != KeepAliveHttpServer::PausingPath && !std::ranges::contains(followupPaths, received.path)) {
      ADD_FAILURE() << "Server received request for unexpected path " << received.path;
      continue;
    }
    std::string expected = "Request for " + received.path;
    EXPECT_EQ(received.body, expected);
  }
}

}

#else // (!)__EMSCRIPTEN__

namespace {
TEST(HttpClient, BasicFunctioning) {
  GTEST_SKIP() << "HttpServer not supported on Emscripten";
}
}

#endif // !__EMSCRIPTEN__
