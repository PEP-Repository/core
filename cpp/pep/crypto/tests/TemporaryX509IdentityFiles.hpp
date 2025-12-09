#pragma once

#include <pep/crypto/X509Certificate.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Filesystem.hpp>

class TemporaryX509IdentityFiles : public pep::X509IdentityFiles {
private:
  pep::filesystem::Temporary mPrivateKeyFile;
  pep::filesystem::Temporary mCertificateChainFile;
  pep::filesystem::Temporary mRootCaFile;

private:
  TemporaryX509IdentityFiles(pep::filesystem::Temporary privateKeyFile, pep::filesystem::Temporary certificateChainFile, pep::filesystem::Temporary rootCaFile)
    : X509IdentityFiles(privateKeyFile.path(), certificateChainFile.path(), rootCaFile.path())
    , mPrivateKeyFile(std::move(privateKeyFile)), mCertificateChainFile(std::move(certificateChainFile)), mRootCaFile(std::move(rootCaFile)) {
  }

public:
  static TemporaryX509IdentityFiles Make(std::filesystem::path privateKeyFile, std::filesystem::path certificateChainFile, std::filesystem::path rootCaFileName) {
    pep::filesystem::Temporary priv(std::move(privateKeyFile));
    pep::filesystem::Temporary cert(std::move(certificateChainFile));
    pep::filesystem::Temporary root(std::move(rootCaFileName));

    auto identity = pep::X509Identity::MakeSelfSigned("X509 testers, inc.", "localhost"); // TODO: allow caller to specify values (and perhaps other certificate metrics)
    pep::WriteFile(priv.path(), identity.getPrivateKey().toPem());
    pep::WriteFile(cert.path(), pep::X509CertificatesToPem(identity.getCertificateChain().certificates()));
    pep::WriteFile(root.path(), identity.getCertificateChain().leaf().toPem());

    return TemporaryX509IdentityFiles(std::move(priv), std::move(cert), std::move(root));
  }
};
