#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <limits>
#include <string>
#include <boost/date_time/posix_time/ptime.hpp>

namespace pep {

/// Represents a point in time with a resolution of milliseconds.
/// @note The C standard does define how time is represented time_t.
///       In practise, time_t almost always has a resolution of whole seconds,
///       making it less precise then our custom type.
class Timestamp final {
public:
  /// Representation of a timezone that is used as parameter in some functions of the Timestamp class
  class TimeZone final {
  public:
    static TimeZone Utc() { return TimeZone{"UTC"}; }
    static TimeZone Local();  ///< The system's current time zone
    static TimeZone PosixTimezone(std::string str) { return TimeZone{std::move(str)}; }

  private:
    friend Timestamp;
    TimeZone(std::string str) : mStr(std::move(str)) {}
    std::string mStr;
  };

  /// The largest (latest) representable point in time.
  static Timestamp Max();

  /// The smallest (earliest) representable point in time, which is the epoch.
  static constexpr Timestamp Min() {
    return Timestamp(0);
  }

  /// Constructs a time point representing the current date and time.
  Timestamp();

  /// Construct a time point from a unix timestamp value.
  /// @param value the number of milliseconds that have elapsed since the epoch, not counting leap seconds.
  constexpr explicit Timestamp(const int64_t value) : mValue(value) {}

  /// Returns the number of milliseconds that have elapsed since the epoch, not counting leap seconds.
  constexpr int64_t getTime() const {
    return mValue;
  }

  /// Converts to an time_t value, by discarding the milliseconds.
  time_t toTime_t() const;

  /// Converts to a Boost ptime
  boost::posix_time::ptime toPtime() const;

  /// Converts to an ISO 8601 datetime string, with a resolution in seconds.
  /// This uses the UTC form with separators between the components.
  /// @example "2024-05-06T08:52:21Z"
  std::string toString() const;

  /// Constructs a pep::Timestamp from a time_t value.
  static Timestamp FromTimeT(::time_t ts);

  /// Constructs a pep::Timestamp from a Boost ptime value.
  static Timestamp FromPtime(boost::posix_time::ptime ts);

  /// Parses a pep::Timestamp value from a XML datetime string (=ISO 8061).
  static Timestamp FromXmlDateTime(const std::string& xml);

  /// Parses a pep::Timestamp value from a yyyyMmDd string, such as "20240523"
  /// @returns the timestamp matching zero milliseconds into the iso date, depending on the timezone
  static Timestamp FromIsoDate(const std::string& yyyyMmDd, TimeZone = TimeZone::Local());

  /// Comparing Timestamps is equivalent to comparing their values.
  auto operator<=>(const Timestamp&) const = default;

private:
  /// milliseconds since UNIX timestamp epoch (only counting non-leap seconds)
  /// Should always be a positive value.
  int64_t mValue;
};


/*
 * \brief Parses (the UTC offset of) the XML time zone specification at the end of a string.
 * \param source The string that may end with the XML time zone specification. If present, the specification is dropped from (the end of) the string.
 * \return The number of minutes east (positive) or west (negative) of UTC that the time zone represents, or std::nullopt if the string doesn't end with a time zone specification.
 * \remark E.g. for a string containing "2025-08-21T15:03:54+02:00" value, this function would return a value of 120,
 *         indicating that the time zone is 2 hours east of UTC (e.g. Amsterdam during DST).
 *         (The "2025-08-21T15:03:54" value that remains in the string represents a wall clock time _within_ the time zone.)
 */
std::optional<std::chrono::minutes> TryExtractXmlTimeZone(std::string& source);

} // namespace pep
