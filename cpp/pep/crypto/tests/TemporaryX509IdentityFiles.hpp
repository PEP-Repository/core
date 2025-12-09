#pragma once

#include <pep/crypto/X509Certificate.hpp>
#include <pep/utils/File.hpp>
#include <pep/utils/Filesystem.hpp>
#include <cassert>

/// @brief Creates a set of (self-signed) X509 identity files that are removed when the instance is destroyed.
/// @remark Defined entirely in the header so that the class can be used from all (unit) test executables.
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

  static pep::filesystem::Temporary CreateTemporary(const std::filesystem::path& directory, const std::string& content) {
    assert(exists(directory));

    // Generate a random nonexistent file path
    std::filesystem::path file;
    const std::string pattern(8U, '%');
    do {
      auto name = pep::filesystem::RandomizedName(pattern);
      file = directory / name;
    } while (exists(file));

    pep::filesystem::Temporary result(file); // Take ownership of the path immediately to ensure that it's deleted even if exceptions occur while writing the file
    pep::WriteFile(result.path(), content);
    return result;
  }

public:
  static TemporaryX509IdentityFiles Make(std::string organization, std::string commonName, std::filesystem::path directory = std::filesystem::current_path()) {
    if (!exists(directory)) {
      throw std::runtime_error("Can't create temporary file in nonexistent directory " + directory.string());
    }

    auto identity = pep::X509Identity::MakeSelfSigned(std::move(organization), std::move(commonName));

    auto priv = CreateTemporary(directory, identity.getPrivateKey().toPem());
    auto cert = CreateTemporary(directory, pep::X509CertificatesToPem(identity.getCertificateChain().certificates()));
    auto root = CreateTemporary(directory, identity.getCertificateChain().leaf().toPem());

    return TemporaryX509IdentityFiles(std::move(priv), std::move(cert), std::move(root));
  }
};
