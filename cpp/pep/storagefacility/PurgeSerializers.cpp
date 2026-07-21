#include <pep/storagefacility/PurgeSerializers.hpp>

namespace pep {

PurgeCycleBeginRequest Serializer<PurgeCycleBeginRequest>::fromProtocolBuffer(proto::PurgeCycleBeginRequest&& source) const {
  return PurgeCycleBeginRequest(PurgeCycle::Definition(std::move(*source.mutable_name())));
}

void Serializer<PurgeCycleBeginRequest>::moveIntoProtocolBuffer(proto::PurgeCycleBeginRequest& dest, PurgeCycleBeginRequest value) const {
  dest.set_name(std::move(value.definition).name());
}

PurgeCycleBeginResponse Serializer<PurgeCycleBeginResponse>::fromProtocolBuffer(proto::PurgeCycleBeginResponse&& source) const {
  return PurgeCycleBeginResponse{
    .id = source.id(),
  };
}

void Serializer<PurgeCycleBeginResponse>::moveIntoProtocolBuffer(proto::PurgeCycleBeginResponse& dest, PurgeCycleBeginResponse value) const {
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

PurgeReviewRequest Serializer<PurgeReviewRequest>::fromProtocolBuffer(proto::PurgeReviewRequest&& source) const {
  return PurgeReviewRequest{
    .id = source.id(),
  };
}

void Serializer<PurgeReviewRequest>::moveIntoProtocolBuffer(proto::PurgeReviewRequest& dest, PurgeReviewRequest value) const {
  dest.set_id(value.id);
}

PurgeConfirmationRequest Serializer<PurgeConfirmationRequest>::fromProtocolBuffer(proto::PurgeConfirmationRequest&& source) const {
  return PurgeConfirmationRequest{
    .id = source.id(),
    .confirmed = source.confirmed(),
  };
}

void Serializer<PurgeConfirmationRequest>::moveIntoProtocolBuffer(proto::PurgeConfirmationRequest& dest, PurgeConfirmationRequest value) const {
  dest.set_id(value.id);
  dest.set_confirmed(value.confirmed);
}

}
