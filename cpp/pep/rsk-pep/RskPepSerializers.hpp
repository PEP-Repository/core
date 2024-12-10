#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>

#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(LocalPseudonym);
PEP_DEFINE_CODED_SERIALIZER(EncryptedLocalPseudonym);
PEP_DEFINE_CODED_SERIALIZER(PolymorphicPseudonym);

}
