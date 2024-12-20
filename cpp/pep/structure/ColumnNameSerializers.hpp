#pragma once

#include <pep/structure/ColumnName.hpp>
#include <pep/serialization/ProtocolBufferedSerializer.hpp>
#include <Messages.pb.h>

namespace pep {

PEP_DEFINE_CODED_SERIALIZER(ColumnNameSection);
PEP_DEFINE_CODED_SERIALIZER(ColumnNameMapping);

}
