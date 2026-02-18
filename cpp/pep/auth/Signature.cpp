#include <pep/auth/Certificate.hpp>
#include <pep/auth/Signature.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <sstream>

using namespace std::chrono;

namespace pep {

Signatory::Signatory(X509CertificateChain certificateChain, X509RootCertificates rootCAs)
  : mCertificateChain(std::move(certificateChain)), mRootCAs(std::move(rootCAs)) {
  if (!mCertificateChain.verify(mRootCAs))
    throw Error("Invalid signatory: certificate chain not trusted");

  const auto& cert = mCertificateChain.leaf();
  if (!IsSigningCertificate(cert)) {
    throw Error("Invalid signatory: certificate is not a (PEP) Signing certificate");
  }
  // TODO: verify intermediate CA

  if (!cert.getCommonName().has_value()) {
    throw Error("Invalid signatory: no common name specified");
  }
  if (!cert.getOrganizationalUnit().has_value()) {
    throw Error("Invalid signatory: no organizational unit specified");
  }
}

std::string Signatory::commonName() const {
  return mCertificateChain.leaf().getCommonName().value();
}

std::string Signatory::organizationalUnit() const {
  return mCertificateChain.leaf().getOrganizationalUnit().value();
}


Signature Signature::Make(std::string_view data, const X509Identity& identity, bool isLogCopy, SignatureScheme scheme) {
  auto timestamp = TimeNow();

  Sha512 hasher;
  hasher.update(PackUint32BE(static_cast<uint32_t>(scheme)));
  if (scheme == SIGNATURE_SCHEME_V3) {
    hasher.update(PackUint64BE(static_cast<uint64_t>(TicksSinceEpoch<milliseconds>(timestamp))));
    hasher.update(data);
  } else if (scheme == SIGNATURE_SCHEME_V4) {
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
  Signatory result(mCertificateChain, rootCAs);

  if (expectedCommonName && *expectedCommonName != result.commonName()) {
    std::ostringstream msg;
    msg << "Invalid signature: incorrect common name on leaf certificate "
        << "(expected " << Logging::Escape(*expectedCommonName) << " but got "
        << Logging::Escape(result.commonName()) << ")";
    throw Error(msg.str());
  }

  auto diff = Abs(mTimestamp - TimeNow());
  if (diff > timestampLeeway) {
    std::ostringstream msg;
    msg << "Invalid signature: timestamp differs by "
        << diff << " with current time; only a difference of "
        << timestampLeeway << " is allowed";
    throw SignatureValidityPeriodError(msg.str());
  }

  if (expectLogCopy && (mScheme < SIGNATURE_SCHEME_V4))
    throw Error("Invalid signature: scheme does not support is_log_copy");
  if (expectLogCopy != mIsLogCopy) {
    if (expectLogCopy)
      throw Error("Invalid signature: expected is_log_copy to be set");
    throw Error("Invalid signature: is_log_copy is set");
  }

  Sha512 hasher;
  hasher.update(PackUint32BE(static_cast<uint32_t>(mScheme)));
  if (mScheme == SIGNATURE_SCHEME_V3) {
    hasher.update(PackUint64BE(static_cast<uint64_t>(TicksSinceEpoch<milliseconds>(mTimestamp))));
    hasher.update(data);
  } else if (mScheme == SIGNATURE_SCHEME_V4) {
    hasher.update(PackUint64BE(static_cast<uint64_t>(TicksSinceEpoch<milliseconds>(mTimestamp))));
    hasher.update(PackUint8(static_cast<uint8_t>(mIsLogCopy)));
    hasher.update(data);
  } else {
    throw Error("Invalid signature: unknown SignatureScheme");
  }

  if (!mCertificateChain.leaf().getPublicKey().verifyDigestSha256(
        hasher.digest().substr(0, 32), mSignature))
    throw Error("Invalid signature: data does not match signature or chain");

  return result;
}

}
