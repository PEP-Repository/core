#include <pep/utils/Log.hpp>
#include <pep/networking/TLSServer.hpp>

#include <boost/bind/bind.hpp>
#include <boost/asio/socket_base.hpp>

namespace pep {

TLSListenerBase::TLSListenerBase(std::shared_ptr<TLSServerParameters> serverParameters, std::shared_ptr<TLSProtocol::Parameters> parameters)
  : acceptor(*parameters->getIOservice(), boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), serverParameters->getListenPort())) {
  // Define TLS context
  parameters->getContext()->use_certificate_chain_file(serverParameters->getTlsCertificateFile().string());
  parameters->getContext()->use_private_key_file(serverParameters->getTlsPrivateKeyFile().string(), boost::asio::ssl::context::pem);
  // Disable older TLS versions
  parameters->getContext()->set_options(
    boost::asio::ssl::context::no_sslv2 |
    boost::asio::ssl::context::no_sslv3 |
    boost::asio::ssl::context::no_tlsv1 |
    boost::asio::ssl::context::no_tlsv1_1
  );
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
    setListenAddress(config.get<std::string>("ListenAddress"));
    setListenPort(config.get<uint16_t>("ListenPort"));

    auto& certificate = *tlsIdentity_.getCertificateChain().cbegin();
    if (!certificate.isServerCertificate()) {
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
