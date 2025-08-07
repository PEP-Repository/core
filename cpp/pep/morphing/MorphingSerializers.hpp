#pragma once

#include <pep/morphing/Metadata.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>

#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_ENUM_SERIALIZER(EncryptionScheme);

PEP_DEFINE_CODED_SERIALIZER(Metadata);
PEP_DEFINE_CODED_SERIALIZER(MetadataXEntry);

}
