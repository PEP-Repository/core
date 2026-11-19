#pragma once

#include <pep/storagefacility/RedactionMessages.hpp>
#include <pep/auth/SigningSerializers.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(RedactionCycle);

PEP_DEFINE_EMPTY_SERIALIZER(RedactionListRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(RedactionListRequest);

PEP_DEFINE_CODED_SERIALIZER(RedactionAnnounceRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(RedactionAnnounceRequest);

PEP_DEFINE_CODED_SERIALIZER(RedactionCancelRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(RedactionCancelRequest);

PEP_DEFINE_CODED_SERIALIZER(RedactionConfirmationRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(RedactionConfirmationRequest);

}
