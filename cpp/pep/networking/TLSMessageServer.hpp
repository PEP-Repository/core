#pragma once

#include <pep/utils/File.hpp>
#include <pep/networking/TLSServer.hpp>

namespace pep {

class TLSMessageServer : public TLSServer<TLSMessageProtocol> {
 protected:
  class Parameters : public TLSServer<TLSMessageProtocol>::Parameters {
  private:
    std::filesystem::path rootCACertificatesFilePath_;
    X509RootCertificates rootCAs_;

  public:
    Parameters(std::shared_ptr<boost::asio::io_service> io_service, 
               const Configuration& config)
      : TLSServer<TLSMessageProtocol>::Parameters(io_service, config),
        rootCACertificatesFilePath_(
          config.get<std::filesystem::path>("CACertificateFile")),
        rootCAs_(ReadFile(rootCACertificatesFilePath_))
    {}


    const std::filesystem::path& getRootCACertificatesFilePath() const noexcept { return rootCACertificatesFilePath_; }
    const X509RootCertificates& getRootCAs() const noexcept { return rootCAs_; }
  };

 private:
  X509RootCertificates mRootCAs;
  std::filesystem::path mRootCAFilePath;

protected:
  TLSMessageServer(std::shared_ptr<Parameters> parameters);

  inline const X509RootCertificates& getRootCAs() const { return mRootCAs; }
  inline std::filesystem::path getRootCAFilePath() const {
    return this->mRootCAFilePath;
  }
  std::filesystem::path ensureDirectoryPath(std::filesystem::path);
  virtual std::optional<std::filesystem::path> getStoragePath();

public:
  static rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> Just(std::shared_ptr<std::string> content) {
    return rxcpp::observable<>::from(rxcpp::observable<>::from(content).as_dynamic());
  }
  template <class T>
  static rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> Just(T content) {
    return rxcpp::observable<>::from(rxcpp::observable<>::from(std::make_shared<std::string>(Serialization::ToString(content))).as_dynamic());
  }
};

class TLSSignedMessageServer : public TLSMessageServer {
public:
  class Parameters : public TLSMessageServer::Parameters {
  private:
    X509IdentityFilesConfiguration signingIdentity_;

  public:
    Parameters(std::shared_ptr<boost::asio::io_service> io_service, const Configuration& config)
      : TLSMessageServer::Parameters(io_service, config), signingIdentity_(config, "PEP") {
    }

    const AsymmetricKey& getPrivateKey() const { return signingIdentity_.getPrivateKey(); }
    const X509CertificateChain& getCertificateChain() const { return signingIdentity_.getCertificateChain(); }

    void check() override {
      auto& certificate = *getCertificateChain().cbegin();
      if (!certificate.isServerCertificate()) {
        throw std::runtime_error("certificateChain's leaf certificate must be a PEP server certificate");
      }
      if (certificate.hasTLSServerEKU()) {
        throw std::runtime_error("certificateChain's leaf certificate must not be a TLS certificate");
      }
      TLSMessageServer::Parameters::check();
    }
  };

private:
  AsymmetricKey mPrivateKey;
  X509CertificateChain mCertificateChain;

protected:
  TLSSignedMessageServer(std::shared_ptr<Parameters> parameters);

  const AsymmetricKey& getPrivateKey() const { return mPrivateKey; }
  const X509CertificateChain& getCertificateChain() const { return mCertificateChain; }

private:
  rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>> handlePingRequest(std::shared_ptr<PingRequest> request);
};

}
