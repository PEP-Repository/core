#pragma once

#include <pep/serialization/NormalizedTypeNaming.hpp>
#include <pep/utils/TypeTraits.hpp>

#include <chrono>
#include <string>
#include <string_view>

// Forward declarations
namespace boost::posix_time { class ptime; };
namespace boost::gregorian { class date; }

namespace pep {

// Same as STL does for system_clock
template<class Duration>
using steady_time = std::chrono::time_point<std::chrono::system_clock, Duration>;
using steady_seconds = steady_time<std::chrono::seconds>;

/// Wall clock timestamp with a precision of milliseconds ('sys_milliseconds').
/// Use steady_time instead if monotonicity is desired.
using Timestamp = std::chrono::sys_time<std::chrono::milliseconds>;

template<DerivedFromSpecialization<std::chrono::time_point> TTimePoint = Timestamp>
[[nodiscard]] auto TimeNow() noexcept {
  return std::chrono::time_point_cast<typename TTimePoint::duration>(TTimePoint::clock::now());
}

template<DerivedFromSpecialization<std::chrono::duration> TToDuration, typename TClock, typename TFromDuration>
[[nodiscard]] constexpr TToDuration::rep TicksSinceEpoch(std::chrono::time_point<TClock, TFromDuration> timePoint) noexcept {
  return std::chrono::duration_cast<TToDuration>(timePoint.time_since_epoch()).count();
}

[[nodiscard]] Timestamp TimestampFromBoostPtime(boost::posix_time::ptime);

/// Parses a timestamp value from an XML (ISO 8061) datetime string (yyyy-mm-ddThh:mm:ss(+-hh:mm))
[[nodiscard]] Timestamp TimestampFromXmlDataTime(std::string_view xml);

[[nodiscard]] boost::posix_time::ptime TimestampToBoostPtime(Timestamp);

/// Converts to an XML (ISO 8601) datetime string, with a resolution in seconds.
/// This uses the UTC form with separators between the components.
/// @example "2024-05-06T08:52:21Z"
[[nodiscard]] std::string TimestampToXmlDateTime(Timestamp);

/// Representation of a timezone that is used as parameter in some functions of the Timestamp class
class TimeZone final {
public:
  [[nodiscard]] static TimeZone Utc() { return TimeZone{"UTC"}; }
  [[nodiscard]] static TimeZone Local();  ///< The system's current time zone
  [[nodiscard]] static TimeZone PosixTimezone(std::string str) { return TimeZone{std::move(str)}; }

  /// Parses a pep::Timestamp value from a yyyyMmDd string, such as "20240523"
  /// @returns the timestamp matching zero milliseconds into the iso date, depending on the timezone
  [[nodiscard]] Timestamp timestampFromYyyyMmDd(std::string_view yyyyMmDd) const;

private:
  TimeZone(std::string str) : mStr(std::move(str)) {}
  std::string mStr;
};

[[nodiscard]] std::chrono::year_month_day BoostDateToStd(const boost::gregorian::date& date);
[[nodiscard]] boost::gregorian::date BoostDateFromStd(const std::chrono::year_month_day& date);

// Make sure serialization remains backward-compatible with the class we used to have, for stored messages
template <>
struct NormalizedTypeNamer<Timestamp> : BasicNormalizedTypeNamer {
  static std::string GetTypeName() { return "Timestamp"; }
};

} // namespace pep
