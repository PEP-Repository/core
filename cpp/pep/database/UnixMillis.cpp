#include <pep/database/UnixMillis.hpp>
#include <boost/numeric/conversion/cast.hpp>

namespace pep::database {

UnixMillis TimestampToUnixMillis(Timestamp timestamp) {
  return boost::numeric_cast<UnixMillis>(TicksSinceEpoch<std::chrono::milliseconds>(timestamp));
}

Timestamp TimestampFromUnixMillis(UnixMillis millis) {
  return Timestamp(std::chrono::milliseconds(boost::numeric_cast<std::chrono::milliseconds::rep>(millis)));
}

}
