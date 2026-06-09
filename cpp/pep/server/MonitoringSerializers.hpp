#pragma once

#include <pep/server/MonitoringMessages.hpp>
#include <pep/auth/SigningSerializers.hpp>

namespace pep {

PEP_DEFINE_EMPTY_SERIALIZER(MetricsRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(MetricsRequest);
PEP_DEFINE_CODED_SERIALIZER(MetricsResponse);

PEP_DEFINE_EMPTY_SERIALIZER(ChecksumChainNamesRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(ChecksumChainNamesRequest);
PEP_DEFINE_CODED_SERIALIZER(ChecksumChainNamesResponse);

PEP_DEFINE_CODED_SERIALIZER(ChecksumChainRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(ChecksumChainRequest);
PEP_DEFINE_CODED_SERIALIZER(ChecksumChainResponse);

}
