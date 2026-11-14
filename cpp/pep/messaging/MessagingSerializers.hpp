#pragma once

#include <pep/auth/SigningSerializers.hpp>
#include <pep/messaging/HousekeepingMessages.hpp>
#include <pep/versioning/Version.hpp>

#include <Messages.pb.h> // for proto::ConfigVersion

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(ConfigVersion);

PEP_DEFINE_CODED_SERIALIZER(PingRequest);
PEP_DEFINE_CODED_SERIALIZER(PingResponse);
PEP_DEFINE_SIGNED_SERIALIZATION(PingResponse);

PEP_DEFINE_EMPTY_SERIALIZER(VersionRequest);
PEP_DEFINE_CODED_SERIALIZER(VersionResponse);

}
