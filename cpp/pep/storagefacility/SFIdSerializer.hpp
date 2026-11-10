#pragma once

#include <pep/crypto/CryptoSerializers.hpp>
#include <pep/storagefacility/SFId.hpp>

#include <Messages.pb.h> // for proto::SFId

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(SFId);
PEP_DEFINE_ENCRYPTED_SERIALIZATION(SFId);

}
