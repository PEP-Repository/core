#include <gtest/gtest.h>

#include <pep/async/IoContextThread.hpp>
#include <pep/httpserver/HTTPServer.hpp>
#include <pep/networking/HttpClient.hpp>
#include <pep/utils/Defer.hpp>

namespace {

const std::string RESPONSE_BODY = "Found someone you have, I would say, hmmm?";

class AsyncHttpServer : boost::noncopyable {
public:
  static constexpr uint16_t PORT = 1880; // Port 80 might be taken by a "real" HTTP server. TODO: try random ports until we find a vacant one

private:
  std::shared_ptr<boost::asio::io_context> mIoContext = std::make_shared<boost::asio::io_context>();
  pep::HTTPServer mServer = pep::HTTPServer(PORT, mIoContext);
  decltype(boost::asio::make_work_guard(*mIoContext)) mWorkGuard = boost::asio::make_work_guard(*mIoContext); // HTTPServer (doesn't keep I/O context busy but) produces HTTP 500 if the I/O context isn't running
  bool mRun = true; // Ensure that the IoContextThread (enters its loop and) runs the I/O context
  pep::IoContextThread mThread = pep::IoContextThread(mIoContext, &mRun);

public:
  AsyncHttpServer() = default;

  ~AsyncHttpServer() noexcept {
    mRun = false; // Prevent the IoContextThread from restarting the I/O context when it runs out of work (when we discard our work guard below)

    mServer.asyncStop();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Give the HTTP server time to finalize

    mWorkGuard.reset(); // Allow the I/O service to stop and...
    mThread.join(); // ...block until (it has done so and) the thread has exited
  }

  template <typename... Args>
  auto registerHandler(Args&&... args) {
    return mServer.registerHandler(std::forward<Args>(args)...);
  }
};

void RegisterAndRetrieve(AsyncHttpServer& server, const std::string& relativeUri, std::shared_ptr<const pep::HTTPResponse> response) {
  ASSERT_EQ(response->getStatusCode(), 200U);

  server.registerHandler(relativeUri, false, [response](const pep::HTTPRequest& request, std::string remoteIp) {return *response; });

  boost::asio::io_context ioContext;

  auto client = pep::networking::HttpClient::Create(pep::networking::HttpClient::Parameters(ioContext, boost::urls::url("http://localhost:" + std::to_string(AsyncHttpServer::PORT) + relativeUri)));
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
        EXPECT_EQ(RESPONSE_BODY, response.getBody());
      },
      [client](std::exception_ptr exception) {client->shutdown(); },
      [client]() {client->shutdown(); }
    );

  ASSERT_NO_FATAL_FAILURE(ioContext.run());

  EXPECT_TRUE(*received) << "Didn't receive a response for HTTP request to " << relativeUri.c_str();
}

}

TEST(HttpClient, BasicFunctioning) {
  AsyncHttpServer server;

  RegisterAndRetrieve(server, "/default", std::make_shared<pep::HTTPResponse>(200U, "OK", RESPONSE_BODY)); // A well-behaved response
  RegisterAndRetrieve(server, "/unsized", std::make_shared<pep::HTTPResponse>(200U, "OK", RESPONSE_BODY, pep::HTTPResponse::HeaderMap(), false)); // No "Content-Length" (or in fact any) header
  // TODO: test HTTPS as well

  // Code below has been disabled to prevent our unit test from requiring a network connection
  // RegisterAndRetrieve(boost::urls::url("https://pep.cs.ru.nl"));
}
