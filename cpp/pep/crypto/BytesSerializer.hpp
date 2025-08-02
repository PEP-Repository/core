#pragma once

#include <pep/crypto/Bytes.hpp>
#include <pep/crypto/CryptoSerializers.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(Bytes);
PEP_DEFINE_ENCRYPTED_SERIALIZATION(Bytes);

}
