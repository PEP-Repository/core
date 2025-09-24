#include <pep/serialization/ErrorSerializer.hpp>

namespace pep {

Error Serializer<Error>::fromProtocolBuffer(proto::Error&& source) const {
  return Error(std::move(*source.mutable_original_type_name()), std::move(*source.mutable_description()));
}

void Serializer<Error>::moveIntoProtocolBuffer(proto::Error& dest, Error value) const {
  *dest.mutable_description() = std::move(value.mDescription);
  *dest.mutable_original_type_name() = value.getOriginalTypeName();
}

}
