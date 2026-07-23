#include <pep/storagefacility/PurgeSerializers.hpp>

namespace pep {

PurgeAnnounceRequest Serializer<PurgeAnnounceRequest>::fromProtocolBuffer(proto::PurgeAnnounceRequest&& source) const {
  return PurgeAnnounceRequest(PurgeCycle::Definition(std::move(*source.mutable_name())));
}

void Serializer<PurgeAnnounceRequest>::moveIntoProtocolBuffer(proto::PurgeAnnounceRequest& dest, PurgeAnnounceRequest value) const {
  dest.set_name(std::move(value.definition).name());
}

PurgeAnnounceResponse Serializer<PurgeAnnounceResponse>::fromProtocolBuffer(proto::PurgeAnnounceResponse&& source) const {
  return PurgeAnnounceResponse{
    .id = source.id(),
  };
}

void Serializer<PurgeAnnounceResponse>::moveIntoProtocolBuffer(proto::PurgeAnnounceResponse& dest, PurgeAnnounceResponse value) const {
  dest.set_id(value.id);
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
