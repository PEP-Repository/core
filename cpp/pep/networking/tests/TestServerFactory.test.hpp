#pragma once

#include <pep/crypto/tests/TemporaryX509IdentityFiles.hpp>
#include <pep/networking/Client.hpp>
#include <pep/networking/Server.hpp>
#include <pep/networking/Tcp.hpp>
#include <pep/networking/Tls.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Filesystem.hpp>

class TestServerFactory {
protected:
  static pep::EndPoint MakeEndPoint(uint16_t port) {
    if (port == pep::networking::TcpBasedProtocol::ServerParameters::RANDOM_PORT) {
      throw std::runtime_error("Server instance is needed for client parameter port determination");
    }
    return pep::EndPoint("localhost", port);
  }

public:
  virtual ~TestServerFactory() noexcept = default;

  virtual const pep::networking::Protocol& protocol() const = 0;
  virtual std::shared_ptr<pep::networking::Protocol::ServerParameters> createServerParameters(boost::asio::io_context& ioContext, uint16_t port) const = 0;

  virtual std::shared_ptr<pep::networking::Protocol::ClientParameters> createClientParameters(const pep::networking::Protocol::ServerParameters& serverParameters) const = 0;

  virtual std::shared_ptr<pep::networking::Protocol::ClientParameters> createClientParameters(const pep::networking::Server& server) const {
    return server.createClientParameters();
  }
};


struct TcpTestServerFactory : public TestServerFactory {
  std::shared_ptr<pep::networking::Protocol::ServerParameters> createServerParameters(boost::asio::io_context& ioContext, uint16_t port) const override {
    return std::make_shared<pep::networking::Tcp::ServerParameters>(ioContext, port);
  }

  std::shared_ptr<pep::networking::Protocol::ClientParameters> createClientParameters(const pep::networking::Protocol::ServerParameters& serverParameters) const override {
    const auto& downcast = dynamic_cast<const pep::networking::Tcp::ServerParameters&>(serverParameters);
    return std::make_shared<pep::networking::Tcp::ClientParameters>(downcast.ioContext(), MakeEndPoint(downcast.port()));
  }

  const pep::networking::Protocol& protocol() const override { return pep::networking::Tcp::Instance(); }
};


class TlsTestServerFactory : public TestServerFactory {
private:
  TemporaryX509IdentityFiles mIdentityFiles;

  void setClientParameterProperties(pep::networking::Tls::ClientParameters& parameters) const {
    parameters.caCertFilePath(mIdentityFiles.getCertificateChainFilePath());
    parameters.skipPeerVerification(true); // Skip (client side) certificate verification: our sample certificate fails it. Curiously the server also flunks the handshake with "tlsv1 alert unknown ca (SSL routines)" if the client doesn't set_verify_mode(boost::asio::ssl::verify_none)
  }

public:
  std::shared_ptr<pep::networking::Protocol::ServerParameters> createServerParameters(boost::asio::io_context& ioContext, uint16_t port) const override {
    auto result = std::make_shared<pep::networking::Tls::ServerParameters>(ioContext, port, mIdentityFiles);
    result->skipCertificateSecurityLevelCheck(true); // Skip (server side) certificate security check: our sample certificate fails OpenSSL's default security level with "ca md too weak"
    return result;
  }

  std::shared_ptr<pep::networking::Protocol::ClientParameters> createClientParameters(const pep::networking::Protocol::ServerParameters& serverParameters) const override {
    const auto& downcast = dynamic_cast<const pep::networking::Tls::ServerParameters&>(serverParameters);
    auto result = std::make_shared<pep::networking::Tls::ClientParameters>(downcast.ioContext(), MakeEndPoint(downcast.port()));
    this->setClientParameterProperties(*result);
    return result;
  }

  std::shared_ptr<pep::networking::Protocol::ClientParameters> createClientParameters(const pep::networking::Server& server) const override {
    auto opaque = TestServerFactory::createClientParameters(server);
    assert(opaque != nullptr);
    auto result = std::dynamic_pointer_cast<pep::networking::Tls::ClientParameters>(opaque);
    if (result == nullptr) {
      throw std::runtime_error("Can't produce client parameters for a non-TLS server");
    }
    this->setClientParameterProperties(*result);
    return result;
  }

  inline TlsTestServerFactory();
  const pep::networking::Protocol& protocol() const override { return pep::networking::Tls::Instance(); }
};


TlsTestServerFactory::TlsTestServerFactory()
  : mIdentityFiles(TemporaryX509IdentityFiles::Make("TLS Test Factory, inc.", "localhost")) {

}
