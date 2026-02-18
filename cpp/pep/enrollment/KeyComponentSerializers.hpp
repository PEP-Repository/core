#pragma once

#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/enrollment/KeyComponentMessages.hpp>

namespace pep {

PEP_DEFINE_ENUM_SERIALIZER(EnrollmentScheme);
PEP_DEFINE_EMPTY_SERIALIZER(KeyComponentRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(KeyComponentRequest);
PEP_DEFINE_CODED_SERIALIZER(KeyComponentResponse);

}
