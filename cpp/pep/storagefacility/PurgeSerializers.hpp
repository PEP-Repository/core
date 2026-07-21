#pragma once

#include <pep/storagefacility/PurgeMessages.hpp>
#include <pep/auth/SigningSerializers.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(PurgeCycleBeginRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(PurgeCycleBeginRequest);
PEP_DEFINE_CODED_SERIALIZER(PurgeCycleBeginResponse);

PEP_DEFINE_CODED_SERIALIZER(PurgeCancelRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(PurgeCancelRequest);
PEP_DEFINE_EMPTY_SERIALIZER(PurgeCancelResponse);

PEP_DEFINE_CODED_SERIALIZER(PurgeReviewRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(PurgeReviewRequest);
PEP_DEFINE_EMPTY_SERIALIZER(PurgeReviewResponse);

PEP_DEFINE_CODED_SERIALIZER(PurgeConfirmationRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(PurgeConfirmationRequest);
PEP_DEFINE_EMPTY_SERIALIZER(PurgeConfirmationResponse);

}
