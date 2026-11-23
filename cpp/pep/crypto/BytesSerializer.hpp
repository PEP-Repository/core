#pragma once

#include <pep/crypto/Bytes.hpp>
#include <pep/crypto/CryptoSerializers.hpp>

#include <Messages.pb.h> // for proto::Bytes

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(Bytes);
PEP_DEFINE_ENCRYPTED_SERIALIZATION(Bytes);

}
