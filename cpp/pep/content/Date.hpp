#pragma once

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <optional>
#include <string>

namespace pep {

/*
 * \brief A Gregorian date without further (time zone or other) information
*/
class Date {
private:
  boost::gregorian::date mGregorian;

public:
  explicit Date(const boost::gregorian::date& gregorian);

  const boost::gregorian::date& gregorian() const noexcept { return mGregorian; }

  static std::optional<Date> TryParseHomebrewFormat(const std::string& value);
  static std::optional<Date> TryParseDdMmYyyy(const std::string& value);
  static bool MatchesDdMmYyyyFormat(const std::string& value) { return TryParseDdMmYyyy(value).has_value(); }

  std::string toHomebrewFormat() const;
};

class DateTime {
private:
  time_t mValue;

public:
  explicit DateTime(time_t value);

  time_t toTimeT() const noexcept { return mValue; }

  static DateTime FromDeviceRecordTimestamp(int64_t timestamp);
  int64_t toDeviceRecordTimestamp() const;
};

}
