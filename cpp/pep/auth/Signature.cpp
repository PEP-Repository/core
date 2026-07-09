#include <pep/auth/Certificate.hpp>
#include <pep/auth/Signature.hpp>
#include <pep/utils/OpenSSLHasher.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Math.hpp>

#include <sstream>

using namespace std::chrono;

namespace pep {

Signatory::Signatory(X509CertificateChain certificateChain, X509RootCertificates rootCAs)
  : certificateChain_(std::move(certificateChain)), rootCAs_(std::move(rootCAs)) {
  if (!certificateChain_.verify(rootCAs_))
    throw Error("Invalid signatory: certificate chain not trusted");

  const auto& cert = certificateChain_.leaf();
  if (!IsSigningCertificate(cert)) {
    throw Error("Invalid signatory: certificate is not a (PEP) Signing certificate");
  }

  if (!cert.getCommonName().has_value()) {
    throw Error("Invalid signatory: no common name specified");
  }
  if (!cert.getOrganizationalUnit().has_value()) {
    throw Error("Invalid signatory: no organizational unit specified");
  }
}

std::string Signatory::commonName() const {
  return certificateChain_.leaf().getCommonName().value();
}

std::string Signatory::organizationalUnit() const {
  return certificateChain_.leaf().getOrganizationalUnit().value();
}


Signature Signature::Make(std::string_view data, const X509Identity& identity, bool isLogCopy, SignatureScheme scheme) {
  auto timestamp = TimeNow();

  Sha512 hasher;
  hasher.update(PackUint32BE(static_cast<uint32_t>(scheme)));
  if (scheme == SignatureScheme::V3) {
    hasher.update(PackUint64BE(static_cast<uint64_t>(TicksSinceEpoch<milliseconds>(timestamp))));
    hasher.update(data);
  } else if (scheme == SignatureScheme::V4) {
    hasher.update(PackUint64BE(static_cast<uint64_t>(TicksSinceEpoch<milliseconds>(timestamp))));
    hasher.update(PackUint8(static_cast<uint8_t>(isLogCopy)));
    hasher.update(data);
  } else {
    throw std::runtime_error("Unknown SignatureScheme");
  }

  return Signature(
    identity.getPrivateKey().signDigestSha256(hasher.digest().substr(0, 32)),
    identity.getCertificateChain(),
    scheme,
    timestamp,
    isLogCopy
  );
}

Signatory Signature::validate(
    std::string_view data,
    const X509RootCertificates& rootCAs,
    std::optional<std::string> expectedCommonName,
    seconds timestampLeeway,
    bool expectLogCopy) const {
  Signatory result(certificateChain_, rootCAs);

  if (expectedCommonName && *expectedCommonName != result.commonName()) {
    std::ostringstream msg;
    msg << "Invalid signature: incorrect common name on leaf certificate "
        << "(expected " << Logging::Escape(*expectedCommonName) << " but got "
        << Logging::Escape(result.commonName()) << ")";
    throw Error(msg.str());
  }

  auto diff = Abs(timestamp_ - TimeNow());
  if (diff > timestampLeeway) {
    std::ostringstream msg;
    msg << "Invalid signature: timestamp differs by "
        << diff << " with current time; only a difference of "
        << timestampLeeway << " is allowed";
    throw SignatureValidityPeriodError(msg.str());
  }

  if (expectLogCopy && (scheme_ < SignatureScheme::V4))
    throw Error("Invalid signature: scheme does not support is_log_copy");
  if (expectLogCopy != isLogCopy_) {
    if (expectLogCopy)
      throw Error("Invalid signature: expected is_log_copy to be set");
    throw Error("Invalid signature: is_log_copy is set");
  }

  Sha512 hasher;
  hasher.update(PackUint32BE(static_cast<uint32_t>(scheme_)));
  if (scheme_ == SignatureScheme::V3) {
    hasher.update(PackUint64BE(static_cast<uint64_t>(TicksSinceEpoch<milliseconds>(timestamp_))));
    hasher.update(data);
  } else if (scheme_ == SignatureScheme::V4) {
    hasher.update(PackUint64BE(static_cast<uint64_t>(TicksSinceEpoch<milliseconds>(timestamp_))));
    hasher.update(PackUint8(static_cast<uint8_t>(isLogCopy_)));
    hasher.update(data);
  } else {
    throw Error("Invalid signature: unknown SignatureScheme");
  }

  if (!certificateChain_.leaf().getPublicKey().verifyDigestSha256(
        hasher.digest().substr(0, 32), signature_))
    throw Error("Invalid signature: data does not match signature or chain");

  return result;
}

}
