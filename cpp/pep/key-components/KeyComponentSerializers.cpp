#include <pep/key-components/KeyComponentSerializers.hpp>
#include <pep/elgamal/ElgamalSerializers.hpp>

namespace pep {

KeyComponentResponse Serializer<KeyComponentResponse>::fromProtocolBuffer(proto::KeyComponentResponse&& source) const {
  KeyComponentResponse result;
  result.pseudonymEncryptionKeyComponent = Serialization::FromProtocolBuffer(std::move(*source.mutable_pseudonymisation_key_component()));
  result.dataEncryptionKeyComponent = Serialization::FromProtocolBuffer(std::move(*source.mutable_encryption_key_component()));
  return result;
}

void Serializer<KeyComponentResponse>::moveIntoProtocolBuffer(proto::KeyComponentResponse& dest, KeyComponentResponse value) const {
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_pseudonymisation_key_component(), value.pseudonymEncryptionKeyComponent);
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_encryption_key_component(), value.dataEncryptionKeyComponent);
}

}
