#pragma once

#include <pep/auth/UserGroup.hpp>
#include <pep/serialization/Serialization.hpp>
#include <Messages.pb.h>

namespace pep {
PEP_DEFINE_CODED_SERIALIZER(UserGroup);
}