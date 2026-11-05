#pragma once

#include <pep/auth/SigningSerializers.hpp>
#include <pep/key-components/KeyComponentMessages.hpp>

#include <Messages.pb.h> // for proto::KeyComponentRequest

namespace pep {

PEP_DEFINE_EMPTY_SERIALIZER(KeyComponentRequest);
PEP_DEFINE_SIGNED_SERIALIZATION(KeyComponentRequest);
PEP_DEFINE_CODED_SERIALIZER(KeyComponentResponse);

}
