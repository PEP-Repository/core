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
    Parameters(std::shared_ptr<boost::asio::io_context> io_context,
               const Configuration& config);


    const std::filesystem::path& getRootCACertificatesFilePath() const noexcept { return rootCACertificatesFilePath_; }
    const X509RootCertificates& getRootCAs() const noexcept { return rootCAs_; }
  };

 private:
  X509RootCertificates mRootCAs;

protected:
  TLSMessageServer(std::shared_ptr<Parameters> parameters);

  inline const X509RootCertificates& getRootCAs() const { return mRootCAs; }
  static std::filesystem::path EnsureDirectoryPath(std::filesystem::path);
  virtual std::optional<std::filesystem::path> getStoragePath();
};

class TLSSignedMessageServer : public TLSMessageServer {
public:
  class Parameters : public TLSMessageServer::Parameters {
  private:
    X509IdentityFilesConfiguration signingIdentity_;

  public:
    Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
      : TLSMessageServer::Parameters(io_context, config), signingIdentity_(config, "PEP") {
    }

    const AsymmetricKey& getPrivateKey() const { return signingIdentity_.getPrivateKey(); }
    const X509CertificateChain& getCertificateChain() const { return signingIdentity_.getCertificateChain(); }

    void check() const override {
      auto& certificate = *getCertificateChain().cbegin();
      if (!certificate.isPEPServerCertificate()) {
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
  MessageBatches handlePingRequest(std::shared_ptr<PingRequest> request);
};

}
