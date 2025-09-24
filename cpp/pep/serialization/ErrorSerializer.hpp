#pragma once

#include <pep/serialization/Error.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>

#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(Error);

}
