#include <pep/crypto/CryptoSerializers.hpp>

namespace pep {

void Serializer<Timestamp>::moveIntoProtocolBuffer(proto::Timestamp& dest, Timestamp value) const {
  dest.set_epoch_millis(value.getTime());
}

Timestamp Serializer<Timestamp>::fromProtocolBuffer(proto::Timestamp&& source) const {
  return Timestamp(source.epoch_millis());
}

Signature Serializer<Signature>::fromProtocolBuffer(proto::Signature&& source) const {
  return Signature(
    std::move(*source.mutable_signature()),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_certificate_chain())),
    Serialization::FromProtocolBuffer(source.scheme()),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_timestamp())),
    source.is_log_copy()
  );
}

void Serializer<Signature>::moveIntoProtocolBuffer(proto::Signature& dest, Signature value) const {
  *dest.mutable_signature() = std::move(value.mSignature);
  Serialization::MoveIntoProtocolBuffer(
    *dest.mutable_certificate_chain(),
    value.mCertificateChain
  );
  dest.set_scheme(Serialization::ToProtocolBuffer(value.mScheme));
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_timestamp(), value.mTimestamp);
  dest.set_is_log_copy(value.mIsLogCopy);
}

X509Certificate Serializer<X509Certificate>::fromProtocolBuffer(proto::X509Certificate&& source) const {
  X509Certificate result;

  mbedtls_x509_crt mTempCert;
  mbedtls_x509_crt_init(&mTempCert);
  if (mbedtls_x509_crt_parse_der(
    &mTempCert,
    reinterpret_cast<const unsigned char*>(source.data().data()),
    source.data().size()
  ) == 0) {
    mbedtls_x509_crt_free(&result.mInternal);
    if (mTempCert.next != nullptr)
      mbedtls_x509_crt_free(mTempCert.next);
    mTempCert.next = nullptr;
    result.mInternal = mTempCert;
  }
  else {
    mbedtls_x509_crt_free(&mTempCert);
  }

  return result;
}

void Serializer<X509Certificate>::moveIntoProtocolBuffer(proto::X509Certificate& dest, X509Certificate value) const {
  dest.set_data(value.mInternal.raw.p, value.mInternal.raw.len);
}

X509CertificateChain Serializer<X509CertificateChain>::fromProtocolBuffer(proto::X509CertificateChain&& source) const {
  X509CertificateChain result;
  Serialization::CopyFromRepeatedProtocolBuffer(result,
    std::move(*source.mutable_certificate()));
  return result;
}

void Serializer<X509CertificateChain>::moveIntoProtocolBuffer(proto::X509CertificateChain& dest, X509CertificateChain value) const {
  Serialization::CopyToRepeatedProtocolBuffer(*dest.mutable_certificate(), std::move(value));
}

X509CertificateSigningRequest Serializer<X509CertificateSigningRequest>::fromProtocolBuffer(proto::X509CertificateSigningRequest&& source) const {
  X509CertificateSigningRequest result;

  mbedtls_x509_csr mTempCertRequest;
  mbedtls_x509_csr_init(&mTempCertRequest);

  if (mbedtls_x509_csr_parse_der(
    &mTempCertRequest,
    reinterpret_cast<const unsigned char*>(source.data().data()),
    source.data().size()
  ) == 0) {
    mbedtls_x509_csr_free(&result.mInternal);
    result.mInternal = mTempCertRequest;
  }
  else {
    //TODO Parse error? Why fail silently?
    mbedtls_x509_csr_free(&mTempCertRequest);
  }

  return result;
}

void Serializer<X509CertificateSigningRequest>::moveIntoProtocolBuffer(proto::X509CertificateSigningRequest& dest, X509CertificateSigningRequest value) const {
  dest.set_data(value.mInternal.raw.p, value.mInternal.raw.len);
}

}
