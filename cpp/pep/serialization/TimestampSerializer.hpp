#pragma once

#include <pep/serialization/ProtocolBufferedSerializer.hpp>
#include <pep/utils/Timestamp.hpp>
#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(Timestamp);

}
