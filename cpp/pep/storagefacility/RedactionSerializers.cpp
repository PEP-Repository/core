#include <pep/serialization/TimestampSerializer.hpp>
#include <pep/storagefacility/RedactionSerializers.hpp>

namespace pep {

namespace {

std::optional<Timestamp> ProtoToOptionalTimestamp(bool present, pep::proto::Timestamp proto) {
  if (!present) {
    return std::nullopt;
  }
  return Serialization::FromProtocolBuffer(std::move(proto));
}

}

RedactionCycle Serializer<RedactionCycle>::fromProtocolBuffer(proto::RedactionCycle&& source) const {
  return RedactionCycle{
    .id = source.id(),
    .definition = RedactionCycle::Definition(std::move(*source.mutable_name())),
    .announced = Serialization::FromProtocolBuffer(std::move(*source.mutable_announced())),
    .applied = ProtoToOptionalTimestamp(source.has_applied(), source.applied()),
    .completed = ProtoToOptionalTimestamp(source.has_completed(), source.completed()),
  };
}

void Serializer<RedactionCycle>::moveIntoProtocolBuffer(proto::RedactionCycle& dest, RedactionCycle value) const {
  dest.set_id(value.id);
  dest.set_name(std::move(value.definition).name());
  Serialization::MoveIntoProtocolBuffer(*dest.mutable_announced(), value.announced);
  if (value.applied.has_value()) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_applied(), *value.applied);
  }
  if (value.completed.has_value()) {
    Serialization::MoveIntoProtocolBuffer(*dest.mutable_completed(), *value.completed);
  }
}


RedactionAnnounceRequest Serializer<RedactionAnnounceRequest>::fromProtocolBuffer(proto::RedactionAnnounceRequest&& source) const {
  return RedactionAnnounceRequest(RedactionCycle::Definition(std::move(*source.mutable_name())));
}

void Serializer<RedactionAnnounceRequest>::moveIntoProtocolBuffer(proto::RedactionAnnounceRequest& dest, RedactionAnnounceRequest value) const {
  dest.set_name(std::move(value.definition).name());
}

RedactionCancelRequest Serializer<RedactionCancelRequest>::fromProtocolBuffer(proto::RedactionCancelRequest&& source) const {
  return RedactionCancelRequest{
    .id = source.id(),
  };
}

void Serializer<RedactionCancelRequest>::moveIntoProtocolBuffer(proto::RedactionCancelRequest& dest, RedactionCancelRequest value) const {
  dest.set_id(value.id);
}

RedactionConfirmationRequest Serializer<RedactionConfirmationRequest>::fromProtocolBuffer(proto::RedactionConfirmationRequest&& source) const {
  return RedactionConfirmationRequest{
    .id = source.id(),
  };
}

void Serializer<RedactionConfirmationRequest>::moveIntoProtocolBuffer(proto::RedactionConfirmationRequest& dest, RedactionConfirmationRequest value) const {
  dest.set_id(value.id);
}

}
