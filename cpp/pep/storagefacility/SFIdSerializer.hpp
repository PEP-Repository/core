#pragma once

#include <pep/storagefacility/SFId.hpp>
#include <pep/crypto/CryptoSerializers.hpp>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(SFId);
PEP_DEFINE_ENCRYPTED_SERIALIZATION(SFId);

}
