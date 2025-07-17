#include <pep/utils/Configuration.hpp>
#include <pep/utils/Log.hpp>
#include <pep/networking/TLSServer.hpp>

#include <boost/bind/bind.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/socket_base.hpp>

namespace pep {

TLSListenerBase::TLSListenerBase(std::shared_ptr<TLSServerParameters> serverParameters, std::shared_ptr<TLSProtocol::Parameters> parameters)
try : acceptor(*parameters->getIoContext(), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), serverParameters->getListenPort())) {
  // Define TLS context
  parameters->getContext()->use_certificate_chain_file(serverParameters->getTlsCertificateFile().string());
  parameters->getContext()->use_private_key_file(serverParameters->getTlsPrivateKeyFile().string(), boost::asio::ssl::context::pem);
} catch (const boost::system::system_error&) { // Also catch exceptions in `acceptor` member initializer
  std::throw_with_nested(std::runtime_error("Could not set up listener on port " + std::to_string(serverParameters->getListenPort())));
}

void TLSListenerBase::listen() {
  std::shared_ptr<TLSProtocol::Connection> new_connection = createConnection();

  boost::asio::socket_base::keep_alive keep_alive_option(true);
  acceptor.set_option(keep_alive_option);

  acceptor.async_accept(new_connection->getSocket(), boost::bind(&TLSListenerBase::accept, this, new_connection, boost::asio::placeholders::error));
}

void TLSListenerBase::accept(std::shared_ptr<TLSProtocol::Connection> new_connection, const boost::system::error_code& error) {
  if (!error) {
    LOG("TLS Server", debug) << "start server handshake with " << new_connection->describe();
    new_connection->startHandshake(boost::asio::ssl::stream_base::server);
  }

  listen();
}

TLSServerParameters::TLSServerParameters(const Configuration& config) : tlsIdentity_(config, "TLS") {
  try {
    setListenPort(config.get<uint16_t>("ListenPort"));

    auto& certificate = *tlsIdentity_.getCertificateChain().cbegin();
    if (!certificate.isPEPServerCertificate()) {
      throw std::runtime_error("certificateChain's leaf certificate must be a server certificate");
    }
    if (!certificate.hasTLSServerEKU()) {
      throw std::runtime_error("certificateChain's leaf certificate must be a TLS certificate");
    }
  }
  catch (const std::exception& e) {
    LOG("TLS Server connection", critical) << "Error with configuration file: " << e.what();
    throw;
  }
}

}
