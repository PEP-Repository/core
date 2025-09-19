#pragma once

#include <pep/networking/Client.hpp>
#include <pep/networking/EndPoint.hpp>
#include <pep/networking/HTTPMessage.hpp>
#include <rxcpp/rx-lite.hpp>
#include <filesystem>

namespace pep::networking {

class HttpClient : public SharedConstructor<HttpClient>, protected LifeCycler, public std::enable_shared_from_this<HttpClient> {
  friend class SharedConstructor<HttpClient>;

public:
  class Parameters {
  public:
    Parameters(boost::asio::io_context& ioContext, boost::urls::url absoluteBase, std::optional<std::string> expectedCommonName = std::nullopt);
    Parameters(boost::asio::io_context& ioContext, bool tls, const EndPoint& endPoint, const std::optional<std::string>& relativeBase = std::nullopt);

    boost::asio::io_context& ioContext() const noexcept { return mIoContext; }
    const boost::urls::url& baseUri() const noexcept { return mBaseUri; }

    const std::optional<std::filesystem::path>& caCertFilepath() const noexcept { return mCaCertFilePath; }
    const std::optional<std::filesystem::path>& caCertFilepath(std::optional<std::filesystem::path> assign) { return mCaCertFilePath = std::move(assign); }

    const ExponentialBackoff::Parameters& reconnectParameters() const noexcept { return mReconnectParameters; }
    const ExponentialBackoff::Parameters& reconnectParameters(ExponentialBackoff::Parameters& assign) noexcept { return mReconnectParameters = assign; }

    std::shared_ptr<Client> createBinaryClient() const;

  private:
    void validateBaseUri() const;

    boost::asio::io_context& mIoContext;
    bool mTls;
    boost::urls::url mBaseUri;
    EndPoint mEndPoint;
    std::optional<std::filesystem::path> mCaCertFilePath;
    ExponentialBackoff::Parameters mReconnectParameters;
  };

private:
  HttpClient(Parameters parameters);

  void stop();
  void restart();

  void onError(std::exception_ptr error);
  void ensureSend();
  void complete(std::exception_ptr error = nullptr);

  void handleRequestPartWritten(const SizedTransfer::Result& result, size_t sentBodyParts);
  void handleReadStatusLine(const DelimitedTransfer::Result& result);
  void readHeaderLine();
  void handleReadHeaderLine(const DelimitedTransfer::Result& result);
  void readBody();
  void readChunkSize();
  void handleReadChunkSize(const DelimitedTransfer::Result& result);
  void handleReadChunk(const SizedTransfer::Result& result);
  void handleReadKnownSizeBody(const SizedTransfer::Result& result);
  void handleReadConnectionBoundBody(const DelimitedTransfer::Result& result);
  void handleReadBody(std::string body);

public:
  ~HttpClient() noexcept override { this->shutdown(); }

  /**
   * @brief Determines if the HttpClient can be used to send requests.
   * @return TRUE if requests can be sent; FALSE if not.
   */
  bool isRunning() const noexcept;

  /**
   * @brief Starts the HttpClient, allowing requests to be sent.
   */
  void start();

  /**
   * @brief Stops the HttpClient, preventing further requests from being sent.
   */
  void shutdown();

  /**
   * @brief Creates a request that can be sent later using the "sendRequest" method.
   * @param method The HTTP method associated with the request.
   * @param path The path (relative to the client's base URI) that the request will access. Specify std::nullopt to access the HttpClient's base URI.
   * @return The HTTP request.
   */
  HTTPRequest makeRequest(HttpMethod method = HttpMethod::GET, const std::optional<std::string>& path = std::nullopt) const;

  /*!
   * \brief Converts a full URL to a path relative to the HttpClient's base URL
   * \param full The (full) URL to extract the path from
   * \return Path relative to the client's base URL
   * \remark Raises an exception for URLs that don't start with the HttpClient's base URL
   */
  std::string pathFromUrl(const boost::urls::url& full);

  /**
   * @brief Sends an HTTP request, returning the response asynchronously.
   * @param request The request to send.
   * @return An observable that emits the server's HTTP response once it's received.
   * @remark The request's URI must match the HttpClient's base URI. For best results, pass HTTPRequest instances produced by the HttpClient's "makeRequest" method.
   */
  rxcpp::observable<HTTPResponse> sendRequest(HTTPRequest request);

  /**
   * @brief Event that is notified when a request is (about to be) sent.
   */
  const Event<HttpClient, std::shared_ptr<const HTTPRequest>> onRequest;

private:
  Parameters mParameters;
  std::shared_ptr<Client> mBinaryClient;
  EventSubscription mBinaryClientConnectionAttempt;
  std::shared_ptr<Connection> mConnection;

  struct PendingRequest {
    std::shared_ptr<HTTPRequest> request;
    rxcpp::subscriber<HTTPResponse> subscriber;
  };

  std::queue<PendingRequest> mPendingRequests;
  bool mSending = false;
  HTTPResponse mResponse;
  std::string mContentBuffer;
};

}
