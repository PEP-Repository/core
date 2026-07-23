#pragma once

#include <pep/storagefacility/PurgeMessages.hpp>
#include <pep/auth/SigningSerializers.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(PurgeCycle);

PEP_DEFINE_CODED_SERIALIZER(PurgeAnnounceRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(PurgeAnnounceRequest);

PEP_DEFINE_CODED_SERIALIZER(PurgeCancelRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(PurgeCancelRequest);

PEP_DEFINE_CODED_SERIALIZER(PurgeConfirmationRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(PurgeConfirmationRequest);

}
