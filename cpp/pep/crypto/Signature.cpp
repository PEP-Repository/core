#include <pep/crypto/Signature.hpp>
#include <pep/utils/Sha.hpp>
#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <sstream>

using namespace std::chrono;

namespace pep {

Signature Signature::create(const std::string& data, X509CertificateChain chain,
      const AsymmetricKey& privateKey, bool isLogCopy, SignatureScheme scheme) {
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
    privateKey.signDigestSha256(hasher.digest().substr(0, 32)),
    std::move(chain),
    scheme,
    timestamp,
    isLogCopy
  );
}

void Signature::assertValid(
    const std::string& data,
    const X509RootCertificates& rootCAs,
    std::optional<std::string> expectedCommonName,
    seconds timestampLeeway,
    bool expectLogCopy) const {
  if (mCertificateChain.begin() == mCertificateChain.end())
    throw Error("Invalid signature: empty certificate chain");
  if (!mCertificateChain.verify(rootCAs))
    throw Error("Invalid signature: certificate chain not trusted");
  if (expectedCommonName && *expectedCommonName != getLeafCertificateCommonName()) {
    std::ostringstream msg;
    msg << "Invalid signature: incorrect common name on leaf certificate "
        << "(expected " << Logging::Escape(*expectedCommonName) << " but got "
        << Logging::Escape(getLeafCertificateCommonName()) << ")";
    throw Error(msg.str());
  }
  if (mCertificateChain.begin()->hasTLSServerEKU())
    throw Error("Invalid signature: TLS certificate used instead of Signing certficiate");

  auto drift = Abs(mTimestamp - TimeNow());
  if (drift > timestampLeeway) {
    std::ostringstream msg;
    msg << "Invalid signature: timestamp differs by "
        << drift << " with current time; only a drift of "
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

  if (!mCertificateChain.begin()->getPublicKey().verifyDigestSha256(
        hasher.digest().substr(0, 32), mSignature))
    throw Error("Invalid signature: data does not match signature or chain");
}


std::string Signature::getLeafCertificateCommonName() const {
  if (mCertificateChain.begin() == mCertificateChain.end())
    return "";
  return mCertificateChain.begin()->getCommonName().value_or("");
}

std::string Signature::getLeafCertificateOrganizationalUnit() const {
  if (mCertificateChain.begin() == mCertificateChain.end())
    return "";
  return mCertificateChain.begin()->getOrganizationalUnit().value_or("");
}

X509Certificate Signature::getLeafCertificate() const {
  if (mCertificateChain.begin() == mCertificateChain.end()) return X509Certificate();
  return *mCertificateChain.begin();
}

}
