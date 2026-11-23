#pragma once

#include <pep/auth/UserGroup.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp> // for PEP_DEFINE_CODED_SERIALIZER

#include <Messages.pb.h>

namespace pep {
PEP_DEFINE_CODED_SERIALIZER(UserGroup);
}