#include <pep/crypto/BytesSerializer.hpp>

namespace pep {

Bytes Serializer<Bytes>::fromProtocolBuffer(proto::Bytes&& source) const {
  return Bytes(std::move(*source.mutable_data()));
}

void Serializer<Bytes>::moveIntoProtocolBuffer(proto::Bytes& dest, Bytes value) const {
  *dest.mutable_data() = std::move(value.mData);
}

} // namespace pep
