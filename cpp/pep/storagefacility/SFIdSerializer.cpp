#include <pep/storagefacility/SFIdSerializer.hpp>

namespace pep {

SFId Serializer<SFId>::fromProtocolBuffer(proto::SFId&& source) const {
  return SFId(std::move(*source.mutable_path()), source.time());
}

void Serializer<SFId>::moveIntoProtocolBuffer(proto::SFId& dest, SFId value) const {
  *dest.mutable_path() = std::move(value.mPath);
  dest.set_time(value.mTime);
}

}
