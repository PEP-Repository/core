#pragma once

#include <chrono>
#include <cmath>
#include <sstream>
#include <string>
#include <iomanip>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace pep::chrono {
namespace detail {
  template<class T>
  void writeFilled(std::ostream& ostream, T num, bool hasPreviousOutput) {
    auto flags = ostream.flags();
    if(hasPreviousOutput) {
      auto width = std::max(static_cast<int>(log10(static_cast<double>(num)) + 1), 2);
      ostream << std::setfill('0') << std::setw(width);
    }
    ostream << num;
    ostream.flags(flags);
  }

  template<typename DurationType, typename Rep, typename Period>
  std::chrono::duration<Rep, Period> writeDurationAs(std::chrono::duration<Rep, Period> in, const std::string& unit, std::ostream& ostream, bool hasPreviousOutput = false) {
    auto converted = duration_cast<DurationType>(in);
    if(converted.count() > 0) {
      writeFilled(ostream, converted.count(), hasPreviousOutput);
      ostream << unit;
    }
    auto retVal = in - duration_cast<std::chrono::duration<Rep, Period>>(converted);
    return retVal;
  }
}

template<typename T>
struct is_duration
  : std::false_type
{ };

template<typename Rep, typename Period>
struct is_duration<std::chrono::duration<Rep, Period>>
  : std::true_type
{ };

class ParseException : public std::runtime_error {
public:
  ParseException(const std::string& reason) : std::runtime_error(reason) {}
};

template<typename T>
T ParseDuration(const std::string& input) {
  std::istringstream inputStream(input);
  uint64_t numericValue{};
  inputStream >> numericValue;
  if(inputStream.fail()) {
    throw ParseException(std::string("Could not parse duration ") + input + ": no numeric value could be read.");
  }
  std::string suffix = std::move(inputStream).str().substr(static_cast<std::string::size_type>(inputStream.tellg()));
  boost::algorithm::trim_left(suffix);
  boost::algorithm::to_lower(suffix);
  if(suffix == "d" || suffix == "day" || suffix == "days") {
    return duration_cast<T>(std::chrono::days(numericValue));
  }
  if(suffix == "h" || suffix == "hour" || suffix == "hours") {
    return duration_cast<T>(std::chrono::hours(numericValue));
  }
  if(suffix == "min" || suffix == "minute" || suffix == "minutes") {
    return duration_cast<T>(std::chrono::minutes(numericValue));
  }
  if(suffix == "s" || suffix == "second" || suffix == "seconds") {
    return duration_cast<T>(std::chrono::seconds(numericValue));
  }
  throw ParseException(std::string("Could not parse duration ") + input + ": unit not recognized.");
}

template<typename Rep, typename Period>
void WriteHumanReadableDuration(std::chrono::duration<Rep, Period> duration, std::ostream& ostream) {
  if(duration.count() == 0) {
    ostream << "0 seconds";
    return;
  }
  auto remaining = detail::writeDurationAs<std::chrono::days>(duration, "d", ostream);
  remaining = detail::writeDurationAs<std::chrono::hours>(remaining, "h", ostream, remaining != duration);
  remaining = detail::writeDurationAs<std::chrono::minutes>(remaining, "m", ostream, remaining != duration);
  auto seconds = duration_cast<std::chrono::duration<double>>(remaining);
  if(seconds.count() > 0) {
    detail::writeFilled(ostream, seconds.count(), remaining != duration);
    ostream << "s";
  }
}

template<typename Rep, typename Period>
std::string ToString(std::chrono::duration<Rep, Period> duration) {
  std::ostringstream oss;
  WriteHumanReadableDuration(duration, oss);
  return std::move(oss).str();
}

}

