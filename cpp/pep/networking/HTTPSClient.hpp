#pragma once

#include <pep/networking/TLSClient.hpp>
#include <pep/networking/HTTPMessage.hpp>
#include <pep/async/AsioReadBuffer.hpp>
#include <pep/utils/Event.hpp>

namespace pep {
//! Class to connect to HTTPS servers. Non-TLS connections are not supported
class HTTPSClient : public TLSClient<TLSProtocol> {
 public:
  using TLSClient<TLSProtocol>::Parameters;

  class Connection : public TLSClient<TLSProtocol>::Connection {
  public:
    Connection(std::shared_ptr<HTTPSClient> client)
      : TLSClient<TLSProtocol>::Connection(client) {
      connectionStatus().subscribe([this](ConnectionStatus status) {
        if (!status.connected && currentConnectionStatus.connected) {
          if (!out.empty()) {
            reconnect();
          }
        }
        currentConnectionStatus = status;
      });
    }

    /*!
     * \brief Event that's notified when an HTTPRequest is (about to be) sent.
     */
    const Event<HTTPSClient, std::shared_ptr<const HTTPRequest>> onRequest;

    /*!
     * \brief Send a HTTP request
     * \param request The Request to send
     * \return Observable that, if no error occurs, emits exactly one Response
     */
    virtual rxcpp::observable<HTTPResponse> sendRequest(std::shared_ptr<HTTPRequest> request);

    /*!
     * \brief Make a GET Request
     *
     * \param path Path to the resource to get
     * \param useBasePath Whether \p path should be relative to the \ref setBasePath "base path" or not
     * \return The created Request
     */
    std::shared_ptr<HTTPRequest> makeGet(const std::string& path, const bool& useBasePath = true) {
      return std::make_shared<HTTPRequest>(getEndPoint().hostname,
        HTTPRequest::GET, boost::urls::url((useBasePath ? basePath : "") + path));
    }

    /*!
     * \brief Make a POST Request
     *
     * \param path Path to the resource to post
     * \param body Body of the Request
     * \param useBasePath Whether \p path should be relative to the \ref setBasePath "base path" or not
     * \return The created Request
     */
    std::shared_ptr<HTTPRequest> makePost(const std::string& path,
      const std::string& body,
      const bool& useBasePath = true) {
      return std::make_shared<HTTPRequest>(getEndPoint().hostname,
        HTTPRequest::POST, boost::urls::url((useBasePath ? basePath : "") + path), body);
    }

    /*!
     * \brief Set the base path
     *
     * Paths of requests will be relative to this base path, unless explicitly set not to use the base path. The base path should not include the hostname (and port) of the endpoint
     *
     * \param basePath The base path
     */
    void setBasePath(const std::string& basePath) {
      HTTPSClient::Connection::basePath = basePath;
    }

    /*!
     * \brief Get a path relative to the base path from a URL
     *
     * URL should match the endpoint of the HTTPSClient.
     *
     * \param url The URL to get a path from
     * \return Path relative to the base path that corresponds with the given URL
     */
    std::string pathFromUrl(std::string url);

    std::string describe() const override { return std::string("HTTPS connection to ") + getEndPoint().describe(); }

  private:
    std::string basePath;
    std::queue<std::pair<std::shared_ptr<HTTPRequest>, rxcpp::subscriber<HTTPResponse>>> out;
    bool requestActive = false;
    HTTPResponse response;
    std::shared_ptr<AsioReadBuffer> mReadBuffer = AsioReadBuffer::Create();
    ConnectionStatus currentConnectionStatus =
      ConnectionStatus(false, boost::system::errc::make_error_code(boost::system::errc::no_message));

    void handleReadStatusline(const boost::system::error_code& error, const std::string& received);
    void handleReadHeaders(const boost::system::error_code& error, const std::string& received);
    void handleReadChunkSize(const boost::system::error_code& error, const std::string& received);
    void handleReadChunk(const boost::system::error_code& error, const std::string& received);
    void handleReadBody(const boost::system::error_code& error, const std::string& received);

  protected:
    //! reconnect to the host
    void reconnect() override;
    //! Called when connection succeeded
    void onConnectSuccess() override;
    //! Called when a network error occurs
    void onError(const boost::system::error_code& error);
    virtual void handleWrite(const boost::system::error_code& error, size_t bytes_transferred);
    void ensureSend();
    void complete();

    std::shared_ptr<HTTPSClient> getClient() const noexcept { return std::static_pointer_cast<HTTPSClient>(TLSClient<TLSProtocol>::Connection::getClient()); }
  };

  HTTPSClient(std::shared_ptr<Parameters> parameters) : TLSClient<TLSProtocol>(parameters) {}

  /*!
   * \brief Send a HTTP request
   * \param request The Request to send
   * \param io_context The io_context to run the request on
   * \return Observable that, if no error occurs, emits exactly one Response
   */
  static rxcpp::observable<HTTPResponse> SendRequest(std::shared_ptr<HTTPRequest> request, std::shared_ptr<boost::asio::io_context> io_context,  const std::optional<std::filesystem::path>& caCertFilepath=std::nullopt);
};

}
