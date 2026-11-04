#include <pep/storagefacility/SFIdSerializer.hpp>
#include <pep/storagefacility/SFId.hpp>
#include <pep/utils/Timestamp.hpp>

#include <Messages.pb.h> // for proto::SFId

#include <cstdint>
#include <utility>

namespace pep {

SFId Serializer<SFId>::fromProtocolBuffer(proto::SFId&& source) const {
  return SFId{std::move(*source.mutable_path()), Timestamp(std::chrono::milliseconds{source.time()})};
}

void Serializer<SFId>::moveIntoProtocolBuffer(proto::SFId& dest, SFId value) const {
  *dest.mutable_path() = std::move(value.path);
  dest.set_time(static_cast<std::uint64_t>(TicksSinceEpoch<std::chrono::milliseconds>(value.time)));
}

}
