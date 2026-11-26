#pragma once

#include <pep/utils/Timestamp.hpp>
#include <cstdint>

namespace pep::database {

/// Timestamp as milliseconds since Unix epoch, used for in database
using UnixMillis = std::int64_t;

UnixMillis TimestampToUnixMillis(Timestamp timestamp);
Timestamp TimestampFromUnixMillis(UnixMillis millis);

}
