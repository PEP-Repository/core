#pragma once

#include <pep/networking/EndPoint.hpp>
#include <pep/networking/ExponentialBackoff.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/networking/TLSProtocol.hpp>

#include <boost/bind/bind.hpp>
#include <filesystem>

#include <boost/type_traits/is_base_of.hpp>

#include <regex>
#include <openssl/ssl.h>

namespace pep {

class TLSClientParameters {
public:
  /*!
  * \param endPoint The EndPoint to connect to
  * \return Nothing
  */
  void setEndPoint(const EndPoint& endPoint) {
    this->endPoint = endPoint;
  }
  //! \return The EndPoint to connect to
  EndPoint getEndPoint() const {
    return endPoint;
  }

  void setCaCertFilepath(const std::filesystem::path& path) {
    if (contextInitialized) {
      throw std::runtime_error("Cannot set CA certificate file path after context has been initialized");
    }
    caCertFilepath = path;
  }

  std::filesystem::path getCaCertFilepath() const {
    return caCertFilepath;
  }

  static bool VerifyCertificateBasedOnExpectedCommonName(
      const std::string& expectedCommonName, 
      bool preverified, 
      boost::asio::ssl::verify_context& verifyCtx);

  static bool IsSpecificSslError(const boost::system::error_code& error, int code);

protected:
  void check() {
    if (endPoint.hostname.empty())
      throw std::runtime_error("EndPoint must have a hostname");
    if (endPoint.port == 0)
      throw std::runtime_error("EndPoint must have a valid port");
  };

  void ensureContextInitialized(boost::asio::ssl::context& context);

private:
  EndPoint endPoint;
  std::filesystem::path caCertFilepath;
  bool contextInitialized = false;
};

template <class TProtocol>
class TLSClient : public TProtocol {
  static_assert(boost::is_base_of<TLSProtocol, TProtocol>::value, "Inherit this class only with TProtocol types that inherit TLSProtocol");
  static_assert(boost::is_base_of<TLSProtocol::Connection, typename TProtocol::Connection>::value, "Inherit this class only with TProtocol types whose Connection inherits TLSProtocol::Connection");
  static_assert(boost::is_base_of<TLSProtocol::Parameters, typename TProtocol::Parameters>::value, "Inherit this class only with TProtocol types whose Parameters inherit TLSProtocol::Parameters");

public:
  class Parameters : public TLSClientParameters, public TProtocol::Parameters {
  public:
    void check() override {
      TLSClientParameters::check();
      TProtocol::Parameters::check();
    }

    void ensureContextInitialized() { TLSClientParameters::ensureContextInitialized(*this->getContext()); }
  };

  class Connection : public TProtocol::Connection {
  public:
    Connection(std::shared_ptr<TLSClient> client) : TProtocol::Connection(client), mBackoff(*client->getIOservice()) {
      initializeCertificateVerification();
    }

    void connect() {
      try {
        auto& endPoint = getEndPoint();
        LOG("TLS Client", debug) << "Connecting to " << endPoint.hostname << ":" << endPoint.port;

        const std::regex hostnameRegex("^([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])(\\.([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9]))*$"); // Copied from https://stackoverflow.com/a/3824105
        const std::regex dottedDecimalRegex("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$"); // Copied from https://stackoverflow.com/a/106223

        // The TLS Server Name Indication extension requires a DNS hostname (as opposed to e.g. an IP address) according to https://tools.ietf.org/html/rfc6066#section-3
        // We "SHOULD check the string syntactically for a dotted-decimal number before [attempting to interpret it as a DNS name]" according to https://tools.ietf.org/html/rfc1123#section-2
        if (!std::regex_match(endPoint.hostname, dottedDecimalRegex)) {
          if (std::regex_match(endPoint.hostname, hostnameRegex)) {
#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wold-style-cast"  // Suppress warning from GCC about C-style cast inside macro
#endif
            SSL_set_tlsext_host_name(this->socket->native_handle(), endPoint.hostname.c_str());
#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif
            LOG("TLS Client", debug) << "Enabled TLS Server Name Indication extension for connection to " << endPoint.hostname << ":" << endPoint.port;
          }
        }

        boost::asio::ip::tcp::resolver resolver(*getClient()->getIOservice());
        boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), endPoint.hostname, std::to_string(endPoint.port));
        boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

        boost::asio::async_connect(this->getSocket(), iterator, boost::bind(&Connection::startClientHandshake, SharedFrom(*this), boost::asio::placeholders::error));
      }
      catch (...) {
        onConnectFailed(boost::system::errc::make_error_code(boost::system::errc::no_message));
      }
    }

  protected:
    std::shared_ptr<TLSClient> getClient() const noexcept { return std::static_pointer_cast<TLSClient>(TProtocol::Connection::getProtocol()); }

    void onConnectSuccess() override {
      mBackoff.success();
      TProtocol::Connection::onConnectSuccess();
    }

    void onConnectFailed(const boost::system::error_code& error) override {
      LOG("TLS Client", severity_level::debug)
        << "TLSClient::Connection::onConnectFailed with "
        << this->describe();
      TProtocol::Connection::onConnectFailed(error);

      if (this->mState == TLSProtocol::Connection::SHUTDOWN)
        return;

      const int timeout = mBackoff.retry(boost::bind(&Connection::reconnect, SharedFrom(*this)));

      auto& endPoint = getEndPoint();
      LOG("TLS Client", warning) << "Retrying connecting to " << endPoint.hostname << ":" << endPoint.port << " in " << timeout << " seconds";
    }

    virtual void reconnect() {
      try {
        resetSocket();
        connect();
      }
      catch (...) {
        onConnectFailed(boost::system::errc::make_error_code(boost::system::errc::no_message));
      }
    }

    const EndPoint& getEndPoint() const noexcept { return getClient()->mEndPoint; }

  private:
    void initializeCertificateVerification() {
      EndPoint endpoint = this->getEndPoint();
      if (endpoint.expectedCommonName.empty()) {
        // use default verification based on hostname
        LOG("TLS Client", debug) << "Using boost's default rfc2818 verification"
          << " for " << endpoint.hostname << ":" << endpoint.port 
          << " instead of our custom code.";
        this->socket->set_verify_callback(
            boost::asio::ssl::rfc2818_verification(endpoint.hostname));
      } else {
        // use our custom verification code based on expected common name
        this->socket->set_verify_callback(
            boost::bind(&TLSClientParameters::VerifyCertificateBasedOnExpectedCommonName, 
              endpoint.expectedCommonName, boost::placeholders::_1, boost::placeholders::_2));
      }
    }

    void startClientHandshake(const boost::system::error_code& error) {
      if (!error) {
        LOG("TLS Client", debug) << "start client handshake with " 
          << getEndPoint().Describe();

        boost::asio::socket_base::keep_alive keep_alive_option(true);
        this->getSocket().set_option(keep_alive_option);

#ifdef __linux__
        int keep_idle = 75; /* seconds */
        setsockopt(this->getSocket().native_handle(), SOL_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
#endif

        TProtocol::Connection::startHandshake(boost::asio::ssl::stream_base::client);
      }
      else {
        LOG("TLS Client", warning) << "Connect failed with " << this->describe()
          << ": " << error.message() << "\n";
        this->onConnectFailed(error);
      }
    }

    void resetSocket() {
      this->mState = TLSProtocol::Connection::UNINITIALIZED;

      // Gracefully shut down the existing socket before discarding it. See https://stackoverflow.com/a/32054476.
      boost::system::error_code ec;
      this->getSocket().cancel(ec);
      this->socket->async_shutdown(
        [socket = this->socket](boost::system::error_code error) {
          if (error
            && !TLSClientParameters::IsSpecificSslError(error, SSL_R_UNINITIALIZED) // SSL initialization was unstarted
            && !TLSClientParameters::IsSpecificSslError(error, SSL_R_SHUTDOWN_WHILE_IN_INIT)) { // SSL initialization/handshaking was started but not completed
            LOG("TLS Client", pep::error) << "Unexpected problem resetting SSL stream: " << error.category().name() << " code " << error.value() << " - " << error.message();
          }

          //Clean up old socket
          socket->lowest_layer().close();
        });

      // Don't wait for the other party to acknowledge our async_shutdown. See https://stackoverflow.com/a/32054476 and https://stackoverflow.com/a/25703699
      [[maybe_unused]] auto buffer = std::make_shared<std::string>("\0"); // Ensure the buffer (1) stays alive for the duration of the async_write operation and (2) has at least 1 character of capacity. See the comments on https://stackoverflow.com/a/25703699
      boost::asio::async_write(*this->socket, boost::asio::buffer(buffer->data(), buffer->size()), [socket = this->socket, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (TLSClientParameters::IsSpecificSslError(error, SSL_R_PROTOCOL_IS_SHUTDOWN)) {
          socket->lowest_layer().close();
        }
      });

      // Create a new socket to retry
      this->socket = getClient()->createSocket();
      initializeCertificateVerification();
    }

    ExponentialBackoff mBackoff;
  };

protected:
  TLSClient(std::shared_ptr<Parameters> parameters) : TProtocol(parameters), mEndPoint(parameters->getEndPoint()) {}

private:
  EndPoint mEndPoint;
};

template <class TClient>
static std::shared_ptr<typename TClient::Connection> CreateTLSClientConnection(std::shared_ptr<typename TClient::Parameters> parameters) {
  static_assert(boost::is_base_of<TLSProtocol, TClient>::value, "TClient must inherit TLSProtocol");
  static_assert(boost::is_base_of<TLSProtocol::Connection, typename TClient::Connection>::value, "Inherit this class only with TClient types whose Connection inherits TLSProtocol::Connection");
  static_assert(boost::is_base_of<TLSProtocol::Parameters, typename TClient::Parameters>::value, "Inherit this class only with TClient types whose Parameters inherit TLSProtocol::Parameters");
  static_assert(boost::is_base_of<TLSClientParameters, typename TClient::Parameters>::value, "Inherit this class only with TClient types whose Parameters inherit TLSClient::Parameters");
  // TODO: assert inheritance from TLSClient<>

  parameters->ensureContextInitialized();

  auto client = std::make_shared<TClient>(parameters);
  auto new_connection = std::make_shared<typename TClient::Connection>(client);
  new_connection->connect();
  return new_connection;
}

}
