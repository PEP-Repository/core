#pragma once

#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/networking/HousekeepingMessages.hpp>
#include <pep/networking/EndPoint.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(ConfigVersion);
PEP_DEFINE_CODED_SERIALIZER(EndPoint);

PEP_DEFINE_CODED_SERIALIZER(PingRequest);
PEP_DEFINE_CODED_SERIALIZER(PingResponse);
PEP_DEFINE_SIGNED_SERIALIZATION(PingResponse);

PEP_DEFINE_EMPTY_SERIALIZER(VersionRequest);
PEP_DEFINE_CODED_SERIALIZER(VersionResponse);

}
