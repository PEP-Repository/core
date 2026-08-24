#include <pep/serialization/TimestampSerializer.hpp>
#include <pep/storagefacility/PurgeSerializers.hpp>

namespace pep {

namespace {

std::optional<Timestamp> ProtoToOptionalTimestamp(bool present, pep::proto::Timestamp proto) {
  if (!present) {
    return std::nullopt;
  }
  return Serialization::FromProtocolBuffer(std::move(proto));
}

}

PurgeCycle Serializer<PurgeCycle>::fromProtocolBuffer(proto::PurgeCycle&& source) const {
  return PurgeCycle{
    .id = source.id(),
    .definition = PurgeCycle::Definition(std::move(*source.mutable_name())),
    .announced = Serialization::FromProtocolBuffer(std::move(*source.mutable_announced())),
    .confirmed = ProtoToOptionalTimestamp(source.has_confirmed(), source.confirmed()),
    .completed = ProtoToOptionalTimestamp(source.has_completed(), source.completed()),
  };
}

void Serializer<PurgeCycle>::moveIntoProtocolBuffer(proto::PurgeCycle& dest, PurgeCycle value) const {
  dest.set_id(value.id);
  dest.set_name(std::move(value.definition).name());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_announced(), value.announced);
  if (value.confirmed.has_value()) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_confirmed(), *value.confirmed);
  }
  if (value.completed.has_value()) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_completed(), *value.completed);
  }
}


PurgeAnnounceRequest Serializer<PurgeAnnounceRequest>::fromProtocolBuffer(proto::PurgeAnnounceRequest&& source) const {
  return PurgeAnnounceRequest(PurgeCycle::Definition(std::move(*source.mutable_name())));
}

void Serializer<PurgeAnnounceRequest>::moveIntoProtocolBuffer(proto::PurgeAnnounceRequest& dest, PurgeAnnounceRequest value) const {
  dest.set_name(std::move(value.definition).name());
}

PurgeCancelRequest Serializer<PurgeCancelRequest>::fromProtocolBuffer(proto::PurgeCancelRequest&& source) const {
  return PurgeCancelRequest{
    .id = source.id(),
  };
}

void Serializer<PurgeCancelRequest>::moveIntoProtocolBuffer(proto::PurgeCancelRequest& dest, PurgeCancelRequest value) const {
  dest.set_id(value.id);
}

PurgeConfirmationRequest Serializer<PurgeConfirmationRequest>::fromProtocolBuffer(proto::PurgeConfirmationRequest&& source) const {
  return PurgeConfirmationRequest{
    .id = source.id(),
  };
}

void Serializer<PurgeConfirmationRequest>::moveIntoProtocolBuffer(proto::PurgeConfirmationRequest& dest, PurgeConfirmationRequest value) const {
  dest.set_id(value.id);
}

}
