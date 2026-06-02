#pragma once

#include <pep/morphing/EnrolledPartyKeys.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/morphing/ServerVerifiers.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>

#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_ENUM_SERIALIZER(EncryptionScheme);
PEP_DEFINE_ENUM_SERIALIZER(EnrollmentScheme);

PEP_DEFINE_CODED_SERIALIZER(Metadata);
PEP_DEFINE_CODED_SERIALIZER(MetadataXEntry);

PEP_DEFINE_CODED_SERIALIZER(ServerVerifiers);

}
