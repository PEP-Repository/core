#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/utils/OpensslUtils.hpp>

namespace pep {

void Serializer<Timestamp>::moveIntoProtocolBuffer(proto::Timestamp& dest, Timestamp value) const {
  dest.set_epoch_millis(TicksSinceEpoch<std::chrono::milliseconds>(value));
}

Timestamp Serializer<Timestamp>::fromProtocolBuffer(proto::Timestamp&& source) const {
  return Timestamp(std::chrono::milliseconds{source.epoch_millis()});
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
  return X509Certificate::FromDer(source.data());
}

void Serializer<X509Certificate>::moveIntoProtocolBuffer(proto::X509Certificate& dest, X509Certificate value) const {
  dest.set_data(value.toDer());
}

X509CertificateChain Serializer<X509CertificateChain>::fromProtocolBuffer(proto::X509CertificateChain&& source) const {
  X509Certificates certificates;
  Serialization::AssignFromRepeatedProtocolBuffer(certificates, std::move(*source.mutable_certificate()));
  return X509CertificateChain(certificates);
}

void Serializer<X509CertificateChain>::moveIntoProtocolBuffer(proto::X509CertificateChain& dest, X509CertificateChain value) const {
  Serialization::AssignToRepeatedProtocolBuffer(*dest.mutable_certificate(), std::move(value).certificates());
}

X509CertificateSigningRequest Serializer<X509CertificateSigningRequest>::fromProtocolBuffer(proto::X509CertificateSigningRequest&& source) const {
  return X509CertificateSigningRequest::FromDer(*source.mutable_data());
}

void Serializer<X509CertificateSigningRequest>::moveIntoProtocolBuffer(proto::X509CertificateSigningRequest& dest, X509CertificateSigningRequest value) const {
  dest.set_data(value.toDer());
}

}
