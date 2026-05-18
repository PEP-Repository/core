#pragma once

#include <pep/auth/Signed.hpp>
#include <Messages.pb.h>

#define PEP_DEFINE_SIGNED_SERIALIZATION(t) \
  PEP_DEFINE_PROTOCOL_BUFFER_SERIALIZATION(Signed<t>, proto::BOOST_PP_CAT(Signed, t))

namespace pep {

PEP_DEFINE_ENUM_SERIALIZER(SignatureScheme);
PEP_DEFINE_CODED_SERIALIZER(Signature);


template <typename T>
class Serializer<Signed<T> > : public ProtocolBufferedMessageSerializer<Signed<T>> {
public:
  using SignedProtocolBufferType = typename ProtocolBufferedSerialization<Signed<T> >::ProtocolBufferType;
  using UnsignedProtocolBufferType = typename ProtocolBufferedSerialization<T>::ProtocolBufferType;
  void moveIntoProtocolBuffer(SignedProtocolBufferType& dest, Signed<T> value) const override;
  Signed<T> fromProtocolBuffer(SignedProtocolBufferType&& source) const override;
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

}
