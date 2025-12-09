#include <pep/networking/CertificateVerification.hpp>
#include <pep/networking/SslError.hpp>
#include <pep/networking/Tls.hpp>
#include <pep/utils/Log.hpp>

#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/bind/bind.hpp>
#include <openssl/ssl.h>
#include <fstream>
#include <regex>

namespace pep::networking {

namespace {

const std::string LOG_TAG = "TLS";

class TlsSocket : public TcpBasedProtocolImplementor<Tls>::Socket {
  friend class pep::networking::Tls;

private:
  boost::asio::ssl::stream<boost::asio::ip::tcp::socket> mImplementor;
  StreamSocket mStreamSocket;
  boost::asio::ssl::stream_base::handshake_type mType;
  bool mShutdownRequired = false;

  void finishClosing();

protected:
  BasicSocket& basicSocket() override { return mImplementor.lowest_layer(); }
  const BasicSocket& basicSocket() const override { return mImplementor.lowest_layer(); }
  StreamSocket& streamSocket() override { return mStreamSocket; }
  void finishConnecting(const ConnectionAttempt::Handler& notify) override;

public:
  TlsSocket(const Tls& protocol, boost::asio::io_context& ioContext, boost::asio::ssl::stream_base::handshake_type type, boost::asio::ssl::context& ssl_context);
  ~TlsSocket() noexcept override;
  void close() override;
};

void TlsSocket::finishConnecting(const ConnectionAttempt::Handler& notify) {
  mShutdownRequired = true; // We may need to close before we've received the handshake callback, at which point we don't know (yet) if OpenSSL has started or even completed its handshaking

  mImplementor.async_handshake(mType, [self = SharedFrom(*this), notify](const boost::system::error_code& error) {
    auto connecting = self->status() == ConnectivityStatus::connecting; // Another ASIO job (e.g. a timer) may have already invoked close() on us

    if (error) {
      self->mShutdownRequired = false; // Handshake didn't succeed: no need to unestablish TLS

      if (connecting) { // Only raise the alarm if handshake failed for a reason other than the socket being closed
        std::ostringstream detail;
        if (error.category() == boost::asio::error::ssl_category) {
          int code = ERR_GET_REASON(static_cast<decltype(ERR_get_error())>(error.value()));
          detail << "OPENSSL error code " << code;
        } else {
          detail << error;
        }
        LOG(LOG_TAG, warning) << "Handshake error with " << self->remoteAddress() << ": " << detail.str() << " " << error.message();

        self->close(); // TODO: specify error as reason
      }

      notify(BoostOperationResult<std::shared_ptr<Protocol::Socket>>(error));
    }
    else if (!connecting) { // Let caller know that we failed to establish connectivity
      notify(ConnectionAttempt::Result::Failure(std::make_exception_ptr(boost::system::system_error(boost::asio::error::connection_aborted))));
    }
    else {
      self->Socket::finishConnecting(notify);
    }
    });
}

TlsSocket::TlsSocket(const Tls& protocol, boost::asio::io_context& ioContext, boost::asio::ssl::stream_base::handshake_type type, boost::asio::ssl::context& ssl_context)
  : Socket(protocol, ioContext), mImplementor(ioContext, ssl_context), mStreamSocket(mImplementor), mType(type) {
}

TlsSocket::~TlsSocket() noexcept {
  if (mShutdownRequired) {
    LOG(LOG_TAG, severity_level::warning) << "Socket wasn't shut down properly"; // Either the owner didn't call close(), or the I/O service was stopped before we could perform our shutdown
  }
}

void TlsSocket::finishClosing() {
  mShutdownRequired = false;
  mImplementor.lowest_layer().close();
  this->setConnectivityStatus(ConnectivityStatus::disconnected);
}

void TlsSocket::close() {
  if (this->status() >= ConnectivityStatus::disconnecting) {
    return;
  }

  if (this->status() != ConnectivityStatus::unconnected) {
    this->setConnectivityStatus(ConnectivityStatus::disconnecting);
  }

  // Cancel pending I/O on the socket
  auto& lowest = mImplementor.lowest_layer();
  if (lowest.is_open()) {
    lowest.cancel();
  }

  // Finish synchronously (don't perform async_shutdown) if TLS was never established
  if (!mShutdownRequired) {
    this->finishClosing();
    return;
  }

  // TLS was established (or we started to do so): gracefully shut down the existing socket before discarding it. See https://stackoverflow.com/a/32054476.
  auto self = SharedFrom(*this);
  mImplementor.async_shutdown([self](boost::system::error_code error) {
    if (error
      && !IsSpecificSslError(error, SSL_R_UNINITIALIZED) // (Our mShutdownRequired has been set, but) SSL initialization was unstarted
      && !IsSpecificSslError(error, SSL_R_SHUTDOWN_WHILE_IN_INIT) // SSL initialization/handshaking was started but not completed
      && !IsSpecificSslError(error, SSL_R_APPLICATION_DATA_AFTER_CLOSE_NOTIFY) // Other party sent us data after (or while) we closed the connection: see https://stackoverflow.com/a/72788966
      && error != boost::asio::error::make_error_code(boost::asio::error::connection_reset) // Other party already closed the connection: see https://stackoverflow.com/a/39162187
      && error != boost::asio::error::make_error_code(boost::asio::error::connection_aborted) // Other party already closed the connection: see https://www.chilkatsoft.com/p/p_299.asp
      ) {
      if (error == boost::asio::ssl::error::make_error_code(boost::asio::ssl::error::stream_errors::stream_truncated)  // remote party [...] closed the underlying transport without shutting down the protocol: see https://stackoverflow.com/a/25703699
        || error == boost::asio::error::make_error_code(boost::asio::error::broken_pipe)) { // happens when you write to a socket fully closed on the other [...] side: see https://stackoverflow.com/a/11866962
        LOG(LOG_TAG, pep::debug) << "Remote party did not properly shut down the connection: " << error.category().name() << " code " << error.value() << " - " << error.message();
      }
      else {
        LOG(LOG_TAG, pep::error) << "Unexpected problem shutting down connection: " << error.category().name() << " code " << error.value() << " - " << error.message();
      }
    }
    self->finishClosing();
    });

  // Don't wait for the other party to acknowledge our async_shutdown. See https://stackoverflow.com/a/32054476 and https://stackoverflow.com/a/25703699
  [[maybe_unused]] auto buffer = std::make_shared<std::string>("\0"); // Ensure the buffer (1) stays alive for the duration of the async_write operation and (2) has at least 1 character of capacity. See the comments on https://stackoverflow.com/a/25703699
  boost::asio::async_write(mImplementor, boost::asio::buffer(buffer->data(), buffer->size()), [self, buffer](const boost::system::error_code& error, std::size_t bytes_transferred) {
    if (IsSpecificSslError(error, SSL_R_PROTOCOL_IS_SHUTDOWN)) {
      self->finishClosing();
    }
    });
}

#ifdef KEYLOG_FILE

std::ofstream keylog;

void keylog_callback(const SSL* ssl, const char* line) {
  keylog << line << std::endl;
}

void set_keylog_file(SSL_CTX* ctx) {
  if (keylog.is_open())
    return;

  keylog.open(KEYLOG_FILE, std::ios::app);
  if (!keylog.is_open()) {
    LOG(LOG_TAG, warning) << "Could not open SSLkeylogfile " << KEYLOG_FILE;
    return;
  }

  SSL_CTX_set_keylog_callback(ctx, keylog_callback);
}

#endif //KEYLOG_FILE

} // End anonymous namespace

std::shared_ptr<TcpBasedProtocol::Socket> Tls::createSocket(TcpBasedProtocol::ClientComponent& component) const {
  auto result = std::make_shared<TlsSocket>(*this, component.ioContext(), boost::asio::ssl::stream_base::client, component.downcastFor(*this).sslContext());

  const auto& endpoint = component.endPoint();
  LOG(LOG_TAG, debug) << "Connecting to " << endpoint.hostname << ":" << endpoint.port;

  std::function<bool(bool, boost::asio::ssl::verify_context&)> verifyCallback;
  if (endpoint.expectedCommonName.empty()) {
    // use default verification based on hostname
    LOG(LOG_TAG, debug) << "Using boost's default hostname verification"
      << " for " << endpoint.hostname << ":" << endpoint.port
      << " instead of our custom code.";
    verifyCallback = boost::asio::ssl::host_name_verification(endpoint.hostname);
  }
  else {
    // use our custom verification code based on expected common name
    verifyCallback = boost::bind(&VerifyCertificateBasedOnExpectedCommonName,
      endpoint.expectedCommonName, boost::placeholders::_1, boost::placeholders::_2);
  }
  result->mImplementor.set_verify_callback(verifyCallback);


  const std::regex hostnameRegex("^([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])(\\.([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9]))*$"); // Copied from https://stackoverflow.com/a/3824105
  const std::regex dottedDecimalRegex("^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$"); // Copied from https://stackoverflow.com/a/106223

  // The TLS Server Name Indication extension requires a DNS hostname (as opposed to e.g. an IP address) according to https://tools.ietf.org/html/rfc6066#section-3
  // We "SHOULD check the string syntactically for a dotted-decimal number before [attempting to interpret it as a DNS name]" according to https://tools.ietf.org/html/rfc1123#section-2
  if (!std::regex_match(endpoint.hostname, dottedDecimalRegex)) {
    if (std::regex_match(endpoint.hostname, hostnameRegex)) {
#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wold-style-cast"  // Suppress warning from GCC about C-style cast inside macro
#endif
      SSL_set_tlsext_host_name(result->mImplementor.native_handle(), endpoint.hostname.c_str());
#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif
      LOG(LOG_TAG, debug) << "Enabled TLS Server Name Indication extension for connection to " << endpoint.hostname << ":" << endpoint.port;
    }
  }

  return result;
}

std::shared_ptr<TcpBasedProtocol::Socket> Tls::createSocket(TcpBasedProtocol::ServerComponent& component) const {
  return std::make_shared<TlsSocket>(*this, component.ioContext(), boost::asio::ssl::stream_base::server, component.downcastFor(*this).sslContext());
}

Tls::NodeComponent::NodeComponent()
  // Accept TLS in general, but...
  : mSslContext(boost::asio::ssl::context::tls) {
  // ...reject older versions
  mSslContext.set_options(
    boost::asio::ssl::context::no_sslv2 |
    boost::asio::ssl::context::no_sslv3 |
    boost::asio::ssl::context::no_tlsv1 |
    boost::asio::ssl::context::no_tlsv1_1
  );
  TrustSystemRootCAs(mSslContext);
}

Tls::ClientComponent::ClientComponent(const ClientParameters& parameters)
  : TcpBasedProtocolImplementor<Tls>::ClientComponent(parameters) {
  auto verify_mode = boost::asio::ssl::verify_peer;
  if (parameters.skipPeerVerification()) {
    LOG(LOG_TAG, pep::warning) << "Skipping OpenSSL peer verification for client socket";
    verify_mode = boost::asio::ssl::verify_none;
  }
  this->sslContext().set_verify_mode(verify_mode);

  const auto& caCertFilePath = parameters.caCertFilePath();
  if (!caCertFilePath.has_value()) {
    this->sslContext().set_default_verify_paths();
  }
  else {
    this->sslContext().load_verify_file(std::filesystem::canonical(*caCertFilePath).string());
  }

#ifdef KEYLOG_FILE
  set_keylog_file(this->sslContext().native_handle());
#endif //KEYLOG_FILE
}

Tls::ServerComponent::ServerComponent(const ServerParameters& parameters)
  : TcpBasedProtocolImplementor<Tls>::ServerComponent(parameters) {
  if (parameters.skipCertificateSecurityLevelCheck()) {
    LOG(LOG_TAG, pep::warning) << "Skipping OpenSSL security level check for certificate";
    SSL_CTX_set_security_level(this->sslContext().native_handle(), 0);
  }

  const auto& identity = parameters.identity();
  this->sslContext().use_certificate_chain_file(identity.getCertificateChainFilePath().string());
  this->sslContext().use_private_key_file(identity.getPrivateKeyFilePath().string(), boost::asio::ssl::context::pem);
}

std::shared_ptr<Protocol::ClientParameters> Tls::createClientParameters(const Protocol::ServerComponent& server) const {
  auto& downcast = server.downcastFor(*this);
  EndPoint endPoint("localhost", downcast.port()); // TODO: set expectedCommonName depending on server (certificate) properties
  return std::make_shared<ClientParameters>(downcast.ioContext(), endPoint); // TODO: set caCertFilePath depending on server (certificate) properties
}

Tls::ClientParameters::ClientParameters(boost::asio::io_context& ioContext, EndPoint endPoint)
  : TcpBasedProtocolImplementor<Tls>::ClientParameters(ioContext, std::move(endPoint)) {
}

Tls::ServerParameters::ServerParameters(boost::asio::io_context& ioContext, uint16_t port, X509IdentityFiles identity)
  : TcpBasedProtocolImplementor<Tls>::ServerParameters(ioContext, port), mIdentity(std::move(identity)) {
}

}
