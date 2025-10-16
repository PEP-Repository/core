#pragma once

#include <pep/utils/TypeTraits.hpp>

#include <chrono>
#include <ctime>
#include <string>
#include <string_view>

// Forward declarations
namespace boost::posix_time { class ptime; };
namespace boost::gregorian { class date; }

namespace pep {

using UnixMillis = std::chrono::milliseconds::rep;

// Same as STL does for system_clock
template<class Duration>
using steady_time = std::chrono::time_point<std::chrono::system_clock, Duration>;
using steady_seconds = steady_time<std::chrono::seconds>;

template<DerivedFromSpecialization<std::chrono::time_point> TTimePoint>
[[nodiscard]] auto TimeNow() noexcept {
  return std::chrono::time_point_cast<typename TTimePoint::duration>(TTimePoint::clock::now());
}

/// Representation of a timezone that is used as parameter in some functions of the Timestamp class
class TimeZone final {
public:
  [[nodiscard]] static TimeZone Utc() { return TimeZone{"UTC"}; }
  [[nodiscard]] static TimeZone Local();  ///< The system's current time zone
  [[nodiscard]] static TimeZone PosixTimezone(std::string str) { return TimeZone{std::move(str)}; }

private:
  friend class Timestamp;
  TimeZone(std::string str) : mStr(std::move(str)) {}
  std::string mStr;
};

/// Wall clock timestamp with a precision of milliseconds.
/// Use steady_time instead if monotonicity is desired.
class Timestamp final : public std::chrono::sys_time<std::chrono::milliseconds> {
public:
  using time_point = std::chrono::sys_time<std::chrono::milliseconds>;
  using time_point::time_point;

  /// Use \c Timestamp::zero or \c Timestamp::now instead
  Timestamp() = delete;

  constexpr Timestamp(const time_point inner) noexcept : time_point(inner) {}

  template<DerivedFromSpecialization<std::chrono::duration> Duration>
  [[nodiscard]] constexpr Duration::rep ticks_since_epoch() const noexcept {
    return duration_cast<Duration>(time_since_epoch()).count();
  }

  [[nodiscard]] static constexpr Timestamp zero() noexcept {
    return Timestamp(duration::zero());
  }

  [[nodiscard]] static Timestamp now() noexcept {
    return TimeNow<time_point>();
  }

  [[nodiscard]] static Timestamp from_time_t(const std::time_t t) noexcept {
    return time_point_cast<duration>(clock::from_time_t(t));
  }

  [[nodiscard]] static Timestamp from_boost_ptime(boost::posix_time::ptime);

  /// Parses a timestamp value from an XML (ISO 8061) datetime string (yyyy-mm-ddThh:mm:ss(+-hh:mm))
  [[nodiscard]] static Timestamp from_xml_date_time(std::string_view xml);

  /// Parses a pep::Timestamp value from a yyyyMmDd string, such as "20240523"
  /// @returns the timestamp matching zero milliseconds into the iso date, depending on the timezone
  [[nodiscard]] static Timestamp from_yyyymmdd(std::string_view yyyyMmDd, const TimeZone& = TimeZone::Local());

  [[nodiscard]] std::time_t to_time_t() const noexcept {
    return clock::to_time_t(*this);
  }

  [[nodiscard]] boost::posix_time::ptime to_boost_ptime() const;

  /// Converts to an XML (ISO 8601) datetime string, with a resolution in seconds.
  /// This uses the UTC form with separators between the components.
  /// @example "2024-05-06T08:52:21Z"
  [[nodiscard]] std::string to_xml_date_time() const;
};

[[nodiscard]] std::chrono::year_month_day BoostDateToStd(const boost::gregorian::date& date);
[[nodiscard]] boost::gregorian::date BoostDateFromStd(const std::chrono::year_month_day& date);

} // namespace pep
