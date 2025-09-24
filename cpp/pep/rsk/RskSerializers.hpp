#pragma once

#include <pep/rsk/Proofs.hpp>
#include <pep/rsk/Verifiers.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>

#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(ScalarMultProof);
PEP_DEFINE_CODED_SERIALIZER(RSKVerifiers);
PEP_DEFINE_CODED_SERIALIZER(RSKProof);

PEP_DEFINE_EMPTY_SERIALIZER(VerifiersRequest);
PEP_DEFINE_CODED_SERIALIZER(VerifiersResponse);

}
