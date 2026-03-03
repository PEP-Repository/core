#include <pep/auth/SigningSerializers.hpp>
#include <pep/crypto/CryptoSerializers.hpp>

namespace pep {

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

}
