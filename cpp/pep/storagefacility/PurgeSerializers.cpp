#include <pep/storagefacility/PurgeSerializers.hpp>

namespace pep {

PurgeDefinitionRequest Serializer<PurgeDefinitionRequest>::fromProtocolBuffer(proto::PurgeDefinitionRequest&& source) const {
  return PurgeDefinitionRequest(std::move(*source.mutable_name()), source.predefined()); // Throws if parameter values are mismatched
}

void Serializer<PurgeDefinitionRequest>::moveIntoProtocolBuffer(proto::PurgeDefinitionRequest& dest, PurgeDefinitionRequest value) const {
  dest.set_name(value.name());
  dest.set_predefined(value.predefined());
}

PurgeDefinitionResponse Serializer<PurgeDefinitionResponse>::fromProtocolBuffer(proto::PurgeDefinitionResponse&& source) const {
  return PurgeDefinitionResponse{
    .id = source.id(),
  };
}

void Serializer<PurgeDefinitionResponse>::moveIntoProtocolBuffer(proto::PurgeDefinitionResponse& dest, PurgeDefinitionResponse value) const {
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
