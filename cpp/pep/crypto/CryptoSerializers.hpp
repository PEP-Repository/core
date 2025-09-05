#pragma once

#include <pep/crypto/Encrypted.hpp>
#include <pep/crypto/Signed.hpp>
#include <pep/crypto/X509Certificate.hpp>
#include <Messages.pb.h>

#define PEP_DEFINE_SIGNED_SERIALIZATION(t) \
  PEP_DEFINE_PROTOCOL_BUFFER_SERIALIZATION(Signed<t>, proto::BOOST_PP_CAT(Signed, t))

#define PEP_DEFINE_ENCRYPTED_SERIALIZATION(t) \
  PEP_DEFINE_PROTOCOL_BUFFER_SERIALIZATION(Encrypted<t>, proto::BOOST_PP_CAT(Encrypted, t))

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(Timestamp);

PEP_DEFINE_ENUM_SERIALIZER(SignatureScheme);
PEP_DEFINE_CODED_SERIALIZER(Signature);

PEP_DEFINE_CODED_SERIALIZER(X509Certificate);
PEP_DEFINE_CODED_SERIALIZER(X509CertificateChain);
PEP_DEFINE_CODED_SERIALIZER(X509CertificateSigningRequest);


template <typename T>
class Serializer<Signed<T> > : public ProtocolBufferedMessageSerializer<Signed<T>> {
public:
  using SignedProtocolBufferType = typename ProtocolBufferedSerialization<Signed<T> >::ProtocolBufferType;
  using UnsignedProtocolBufferType = typename ProtocolBufferedSerialization<T>::ProtocolBufferType;
  void moveIntoProtocolBuffer(SignedProtocolBufferType& dest, Signed<T> value) const override;
  Signed<T> fromProtocolBuffer(SignedProtocolBufferType&& source) const override;
};

template <typename T>
class Serializer<Encrypted<T> > : public ProtocolBufferedMessageSerializer<Encrypted<T>> {
public:
  using EncryptedProtocolBufferType = typename ProtocolBufferedSerialization<Encrypted<T> >::ProtocolBufferType;
  using UnsignedProtocolBufferType = typename ProtocolBufferedSerialization<T>::ProtocolBufferType;
  void moveIntoProtocolBuffer(EncryptedProtocolBufferType& dest, Encrypted<T> value) const override;
  Encrypted<T> fromProtocolBuffer(EncryptedProtocolBufferType&& source) const override;
};


template <typename T>
void Serializer<Signed<T> >::moveIntoProtocolBuffer(
  SignedProtocolBufferType& dest, Signed<T> value) const {
  dest.set_data(std::move(value.mData));
  Serialization::MoveIntoProtocolBuffer(
    *dest.mutable_signature(),
    std::move(value.mSignature)
  );
}

template <typename T>
Signed<T> Serializer<Signed<T> >::fromProtocolBuffer(SignedProtocolBufferType&& source) const {
  return Signed<T>(
    std::move(*source.mutable_data()),
    Serialization::FromProtocolBuffer(std::move(*source.mutable_signature()))
  );
}

template <typename T>
void Serializer<Encrypted<T> >::moveIntoProtocolBuffer(
  EncryptedProtocolBufferType& dest, Encrypted<T> value) const {
  dest.set_ciphertext(std::move(value.mCiphertext));
  dest.set_iv(std::move(value.mIv));
  dest.set_tag(std::move(value.mTag));
}

template <typename T>
Encrypted<T> Serializer<Encrypted<T> >::fromProtocolBuffer(EncryptedProtocolBufferType&& source) const {
  return Encrypted<T>(
    std::move(*source.mutable_ciphertext()),
    std::move(*source.mutable_iv()),
    std::move(*source.mutable_tag()));
}

}
