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
  : mPrivateKeyFile("private.key"), mCertificateChainFile("certificate.chain") {

  // Generated using https://8gwifi.org/SelfSignCertificateFunctions.jsp

  pep::WriteFile(mPrivateKeyFile.path(),
    "-----BEGIN RSA PRIVATE KEY-----" "\n"
    "MIIEpAIBAAKCAQEA4BzfwWL9Vbgs2eXUEP/IF5GOvtZCqQ/CN9MBG/HCOLBV1v+L" "\n"
    "zVEOT9YWWCepLiwjCLpbpQwVCmVcjP9GxVAa1CAsOojruz8yNaRIgGFKU4xsPjys" "\n"
    "0T8Pr2WhP/hp2Y4vx7XGQLwUQ+7NpwP5QSc335rxC/j/5rdgeWP6Du0LlMUsRorv" "\n"
    "q1e26s6vHLeK5DMYU770jgUcdo+E3sHgtZWN/kQufYlYg5LuIg4gU18PIGdsCFvo" "\n"
    "I6gr4DruanKxfHfESuTENdu/A6CxAstqbEWlKY0nY32HCkBdAXkJXyKTeK8XD7FP" "\n"
    "GkVkBoW5oYlrV5+/0T9Nm7wfINTsvdR4kNc6nwIDAQABAoIBAAZtvZtmmAbDYV1p" "\n"
    "VRzBuGzdOK09cXXuwJONy7xrr9sSlGl/NPSjtanjnJydwFiw7v97Kr9lDrAGcL+0" "\n"
    "FqBB6DRAP7kdRMnRd8K+lZfUBOgCM4obfweVR28eX9cd2lCYXlWA37V9hoVK6dpJ" "\n"
    "+mQzapU+9KJzNv/9C1kyXXrb/E3YfTPKvHhWV+mTqETe8lmdy7+d4FoSBTwYvWJv" "\n"
    "NSX/WhUUCnS8apEpkE5gbBLoVfZlHEVNtbJpzkJUFXqEJAr5cewcJeQbySoHKboy" "\n"
    "h1B1MgNHrrgqnHb0EHsR68967WkZ3hzrmtv2J1g3HLiyU2NqdTm2ts77uLrBTZEG" "\n"
    "1c21P+ECgYEA8rCCU3cde03yVWwf6b15/My9ENhTxbVLFmnS4VN0N/XMSeie7KHm" "\n"
    "IExrVca1fPn0QlAnRYhDFZbjEwsFBk+gYsILvZqq2kgLRlA4VeF5CEKANpD1FSVa" "\n"
    "//QU/It5Zzy3IRMhByV/Xrp4WQhgvhEx29J3iKrKTW0LBTBQ2c2TR78CgYEA7GeJ" "\n"
    "wZzxS+CKRuxGnSyUBKRxc1pTo0n9qWge3Jd90UWGtj6PGo3/NEQXeJhWTsA4iU6T" "\n"
    "N/mC21fbhqD9hSqijxtfZFF8ffcTscVcbvRr4j0WOF09r6uIcUaHni2j72ibINK6" "\n"
    "Y6imU/TKBxvDnQmPcsVt9+R9fJYMpR9VHDabxSECgYBBbCnF3E97RPj15C76MNTo" "\n"
    "vDyfhOGYY5X5Vc++ZGPpDf7jUa099yr1PASXW/ji1vLsyXS8vs3uzP0rzgWtvNts" "\n"
    "pAjMNRynuVIow0lchWq+Okcb7pnS+H3+j8r0hZjVpr1rUh/OMGKUo8n7nlGOC06Y" "\n"
    "hrUoh3n/w0x8OpkhDdUNOwKBgQDScJyFOFLn63rMBZoaYctlkojXWYnoan4epmwK" "\n"
    "i+RZPN3dLzUuO0b5XL/T/y+dLKlnOQX+JuMgpEXrwzXKrBhG8ePppkv+ycnDTt+o" "\n"
    "eXXrz9sO05mM3lI4G8OvwAsVm/Wzs0JuYnulctvAlit8iD0kurDYoZI/LEcXWhvm" "\n"
    "YIorYQKBgQCujUKcEx10IB4rd5EjFki5jjk8oVx5lbZ/BhSIws6/IOLyGR7eH+t9" "\n"
    "dkV4BhwFqVntzKXHxcdy18H/nmnHU6t6/6pVvU1kfVG8yr3b+Cr9Vx4VJOqjTuKF" "\n"
    "2F/X3ofH1zRtBXPAKisUNnJIdi8V0T2FQW73rH7Ix5kHKP0b4O4PXg==" "\n"
    "-----END RSA PRIVATE KEY-----" "\n"
    );

  pep::WriteFile(mCertificateChainFile.path(),
    "-----BEGIN CERTIFICATE-----" "\n"
    "MIIDiTCCAnGgAwIBAgIgF2lRAqINqhIw4SM2GgVGVB7TN8BYzn0JDw6OBSIQEfIw" "\n"
    "DQYJKoZIhvcNAQEFBQAwWjEJMAcGA1UEBhMAMQkwBwYDVQQKDAAxCTAHBgNVBAsM" "\n"
    "ADESMBAGA1UEAwwJbG9jYWxob3N0MQ8wDQYJKoZIhvcNAQkBFgAxEjAQBgNVBAMM" "\n"
    "CWxvY2FsaG9zdDAeFw0yNDA4MTQxMjU5MjdaFw0zNDA4MTUxMjU5MjdaMEYxCTAH" "\n"
    "BgNVBAYTADEJMAcGA1UECgwAMQkwBwYDVQQLDAAxEjAQBgNVBAMMCWxvY2FsaG9z" "\n"
    "dDEPMA0GCSqGSIb3DQEJARYAMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC" "\n"
    "AQEA4BzfwWL9Vbgs2eXUEP/IF5GOvtZCqQ/CN9MBG/HCOLBV1v+LzVEOT9YWWCep" "\n"
    "LiwjCLpbpQwVCmVcjP9GxVAa1CAsOojruz8yNaRIgGFKU4xsPjys0T8Pr2WhP/hp" "\n"
    "2Y4vx7XGQLwUQ+7NpwP5QSc335rxC/j/5rdgeWP6Du0LlMUsRorvq1e26s6vHLeK" "\n"
    "5DMYU770jgUcdo+E3sHgtZWN/kQufYlYg5LuIg4gU18PIGdsCFvoI6gr4DruanKx" "\n"
    "fHfESuTENdu/A6CxAstqbEWlKY0nY32HCkBdAXkJXyKTeK8XD7FPGkVkBoW5oYlr" "\n"
    "V5+/0T9Nm7wfINTsvdR4kNc6nwIDAQABo08wTTAdBgNVHQ4EFgQUxcu1RiYZn18H" "\n"
    "wsk5p/MqwPxDC5EwHwYDVR0jBBgwFoAUxcu1RiYZn18Hwsk5p/MqwPxDC5EwCwYD" "\n"
    "VR0RBAQwAoIAMA0GCSqGSIb3DQEBBQUAA4IBAQAVShpneFvoRSiqIwRhzYwK7wVW" "\n"
    "FV740F03aqQJWd/dQ08dAz4Q9woRZdueh1D1KrijvElU4xxGmFvfrf61pKh7FXiO" "\n"
    "s4ZgTg0IzEJGago1TF0z+jp9sAWbXH8oPWqkL7FMjVrsEwfgUmnKb0QPf55ew0XH" "\n"
    "1LjJMafs21oy8ukYY6Fe2PZ3hTmuI4Mt8czp0h1UWsRbCSgCJNKRn479hzIyLQhW" "\n"
    "5w6z5gTYJdFOJhO8jZPGa141MnVMmCvVtMVL8BGJv47CMBNZBBmROxEqE9hwRJaM" "\n"
    "j/2cvryh9PFVyQNU6OC3Vn9dCzA3gnpOPRZjDRxyBC9D9/sUDeWkmQrAS65e" "\n"
    "-----END CERTIFICATE-----" "\n"
  );

  throw std::runtime_error("Write root CA file here");
}
