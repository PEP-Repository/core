#include <pep/database/UnixMillis.hpp>
#include <pep/utils/Timestamp.hpp>

#include <boost/numeric/conversion/cast.hpp>
#include <chrono>

namespace pep::database {

UnixMillis TimestampToUnixMillis(Timestamp timestamp) {
  return boost::numeric_cast<UnixMillis>(TicksSinceEpoch<std::chrono::milliseconds>(timestamp));
}

Timestamp TimestampFromUnixMillis(UnixMillis millis) {
  return Timestamp(std::chrono::milliseconds(boost::numeric_cast<std::chrono::milliseconds::rep>(millis)));
}

}
