#include <pep/serialization/TimestampSerializer.hpp>

namespace pep {

void Serializer<Timestamp>::moveIntoProtocolBuffer(proto::Timestamp& dest, Timestamp value) const {
  dest.set_epoch_millis(TicksSinceEpoch<std::chrono::milliseconds>(value));
}

Timestamp Serializer<Timestamp>::fromProtocolBuffer(proto::Timestamp&& source) const {
  return Timestamp(std::chrono::milliseconds{ source.epoch_millis() });
}

}
