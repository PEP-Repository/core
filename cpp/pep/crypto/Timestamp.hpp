#pragma once

#include <cstdint>
#include <ctime>
#include <limits>
#include <string>

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
  static constexpr Timestamp max() {
    return Timestamp(std::numeric_limits<int64_t>::max());
  }

  /// The smallest (earliest) representable point in time, which is the epoch.
  static constexpr Timestamp min() {
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

  /// Converts to an ISO 8601 datetime string, with a resolution in seconds.
  /// This uses the UTC form with separators between the components.
  /// @example "2024-05-06T08:52:21Z"
  std::string toString() const;

  /// Constructs a pep::Timestamp from a time_t value.
  static Timestamp FromTimeT(::time_t ts);

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

} // namespace pep
