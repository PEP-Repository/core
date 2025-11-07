#include <pep/pullcastor/PullCastorUtils.hpp>
#include <boost/property_tree/ptree.hpp>

#include <chrono>
//XXX Remove when std timezones are widely supported
#if __cpp_lib_chrono < 201907L
#include <date/tz.h>
using date::parse;
using date::local_seconds;
using date::zoned_seconds;
#else
using std::chrono::parse;
using std::chrono::local_seconds;
using std::chrono::zoned_seconds;
#endif

namespace pep {
namespace castor {

namespace {

template<typename TClock>
void ParseDateTime(std::string datestring, const std::string& formatstring, std::chrono::time_point<TClock, std::chrono::seconds>& datetime) {
  std::istringstream datestringstream(std::move(datestring));
  // std::chrono::locate_zone()
  datestringstream >> parse(formatstring, datetime);
  if (datestringstream.fail()) {
    throw std::runtime_error("Error parsing date");
  }
}

}

Timestamp ParseCastorDateTime(const boost::property_tree::ptree& datetimeObject) {
  // e.g. "2020-11-19 13:49:44.000000"
  const std::string& datestring = datetimeObject.get<std::string>("date");

  local_seconds datetime;
  //date/tz.hpp doesn't handle the fractional second part well, so we strip it off
  ParseDateTime(datestring.substr(0, datestring.find('.')), "%F %T", datetime);

  // timezone: e.g. "Europe/Amsterdam"
  const std::string& timezone = datetimeObject.get<std::string>("timezone");

  return zoned_seconds(timezone, datetime).get_sys_time();
}

}
}
