#pragma once

#include <pep/rsk/Proofs.hpp>
#include <pep/rsk/Verifiers.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>

#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(ScalarMultProof);
PEP_DEFINE_CODED_SERIALIZER(ReshuffleRekeyVerifiers);
PEP_DEFINE_CODED_SERIALIZER(RskProof);

PEP_DEFINE_EMPTY_SERIALIZER(VerifiersRequest);
PEP_DEFINE_CODED_SERIALIZER(VerifiersResponse);

}
