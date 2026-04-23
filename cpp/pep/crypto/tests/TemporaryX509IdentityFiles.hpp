#pragma once

#include <pep/crypto/X509Certificate.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Filesystem.hpp>
#include <cassert>

/// @brief Creates a set of (self-signed) X509 identity files that are removed when the instance is destroyed.
/// @remark Header-only so that the class can be used from all (unit test executable) binaries without additional link requirements.
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
  static TemporaryX509IdentityFiles Make(std::string_view organization, std::string_view commonName) {
    auto identity = pep::X509Identity::MakeSelfSigned(organization, commonName);

    auto priv = pep::filesystem::Temporary::MakeFile(identity.getPrivateKey().toPem());
    auto cert = pep::filesystem::Temporary::MakeFile(pep::X509CertificatesToPem(identity.getCertificateChain().certificates()));
    auto root = pep::filesystem::Temporary::MakeFile(identity.getCertificateChain().leaf().toPem());

    return TemporaryX509IdentityFiles(std::move(priv), std::move(cert), std::move(root));
  }

  /// @brief Gets a sliced copy of this instance, e.g. to pass to Tls::ServerParameters. This instance must outlive the sliced copy.
  /// @return An X509IdentityFiles instance corresponding to this (TemporaryX509IdentityFiles) instance.
  /// @remark Helps get rid of linting error "cppcoreguidelines-slicing". See https://stackoverflow.com/a/59867897.
  X509IdentityFiles slicedToX509IdentityFiles() const {
    return X509IdentityFiles(mPrivateKeyFile.path(), mCertificateChainFile.path(), mRootCaFile.path());
  }
};
