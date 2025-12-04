#pragma once

#include <pep/networking/Client.hpp>
#include <pep/networking/Server.hpp>
#include <pep/networking/Tcp.hpp>
#include <pep/networking/Tls.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Filesystem.hpp>

class TestServerFactory {
private:
  std::shared_ptr<pep::networking::Server> mServer;

  std::shared_ptr<pep::networking::Server> server() const {
    if (mServer == nullptr) {
      throw std::runtime_error("No server has been created yet");
    }
    return mServer;
  }

protected:
  virtual std::shared_ptr<pep::networking::Protocol::ServerParameters> createServerParameters(boost::asio::io_context& ioContext, uint16_t port) const = 0;

  virtual std::shared_ptr<pep::networking::Protocol::ClientParameters> createClientParameters(const pep::networking::Server& server) const {
    return this->server()->createClientParameters();
  }

public:
  virtual ~TestServerFactory() noexcept = default;

  virtual std::string protocolName() const = 0;

  std::shared_ptr<pep::networking::Server> createServer(boost::asio::io_context& ioContext, uint16_t port) {
    if (mServer != nullptr) {
      throw std::runtime_error("Server already created");
    }
    auto parameters = this->createServerParameters(ioContext, port);
    return mServer = pep::networking::Server::Create(*parameters);
  }

  std::shared_ptr<pep::networking::Client> createClient() const {
    auto parameters = this->createClientParameters(*this->server());
    return pep::networking::Client::Create(*parameters);
  }
};


class TcpTestServerFactory : public TestServerFactory {
protected:
  std::shared_ptr<pep::networking::Protocol::ServerParameters> createServerParameters(boost::asio::io_context& ioContext, uint16_t port) const override {
    return std::make_shared<pep::networking::Tcp::ServerParameters>(ioContext, port);
  }

public:
  std::string protocolName() const override { return pep::networking::Tcp::Instance().name(); }
};


class TlsTestServerFactory : public TestServerFactory {
private:
  pep::filesystem::Temporary mPrivateKeyFile;
  pep::filesystem::Temporary mCertificateChainFile;
  pep::filesystem::Temporary mRootCaFile;

protected:
  std::shared_ptr<pep::networking::Protocol::ServerParameters> createServerParameters(boost::asio::io_context& ioContext, uint16_t port) const override {
    auto result = std::make_shared<pep::networking::Tls::ServerParameters>(ioContext, port, pep::X509IdentityFilesConfiguration(mPrivateKeyFile.path(), mCertificateChainFile.path(), mRootCaFile.path()));
    result->skipCertificateSecurityLevelCheck(true); // Skip (server side) certificate security check: our sample certificate fails OpenSSL's default security level with "ca md too weak"
    return result;
  }

  std::shared_ptr<pep::networking::Protocol::ClientParameters> createClientParameters(const pep::networking::Server& server) const override {
    auto result = std::static_pointer_cast<pep::networking::Tls::ClientParameters>(TestServerFactory::createClientParameters(server));
    result->caCertFilePath(mCertificateChainFile.path());
    result->skipPeerVerification(true); // Skip (client side) certificate verification: our sample certificate fails it. Curiously the server also flunks the handshake with "tlsv1 alert unknown ca (SSL routines)" if the client doesn't set_verify_mode(boost::asio::ssl::verify_none)
    return result;
  }

public:
  inline TlsTestServerFactory();
  std::string protocolName() const override { return pep::networking::Tls::Instance().name(); }
};


TlsTestServerFactory::TlsTestServerFactory()
  : mPrivateKeyFile("private.key"), mCertificateChainFile("certificate.chain"), mRootCaFile("rootCA.cert") {

  auto identity = pep::X509Identity::MakeSelfSigned("TLS testers, inc.", "PepTlsUnitTest");
  pep::WriteFile(mPrivateKeyFile.path(), identity.getPrivateKey().toPem());
  pep::WriteFile(mCertificateChainFile.path(), pep::X509CertificatesToPem(identity.getCertificateChain().certificates()));
  pep::WriteFile(mRootCaFile.path(), identity.getCertificateChain().leaf().toPem());
}
