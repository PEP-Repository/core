#include <pep/crypto/Timestamp.hpp>

#include <pep/utils/StringStream.hpp>

#include <format>
#include <regex>
#include <sstream>
#include <stdexcept>

#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/date_time/gregorian/greg_date.hpp>
#include <boost/date_time/gregorian/gregorian_io.hpp>
#include <boost/date_time/local_time/local_date_time.hpp>
#include <boost/date_time/local_time/local_time_io.hpp>
#include <boost/make_shared.hpp>
#include <boost/numeric/conversion/cast.hpp>

using namespace std::chrono;

namespace bpt = boost::posix_time;
namespace blt = boost::local_time;

namespace pep {

namespace {

constexpr auto SYSTEM_LOCAL_TIME_ZONE = "SYSTEM_TIME_ZONE";

const bpt::ptime UnixEpochPtime(BoostDateFromStd(sys_days{}));

/// Converts strings to pep::Timestamp values
class TimestampParser {
public:
  virtual ~TimestampParser() = default;

  /// Parses the input that was passed at construction.
  Timestamp parse(std::string_view input) {
    try {
      return parseImpl(input);
    } catch (...) {
      std::throw_with_nested(std::runtime_error(std::format("Couldn't parse \"{}\" as {}", input, mFormatName)));
    }
  }

protected:
  explicit TimestampParser(std::string name)
    : mFormatName{std::move(name)} {}

  virtual Timestamp parseImpl(std::string_view) = 0; ///< actual parsing of the string

private:
  std::string mFormatName;
};

class XmlDateTimeParser : public TimestampParser {
public:
  XmlDateTimeParser() : TimestampParser{"xml date-time"} {}

protected:
  Timestamp parseImpl(std::string_view str) override {
    // See:
    // - https://www.boost.org/doc/libs/latest/doc/html/date_time/date_time_io.html#date_time.format_flags
    // - https://www.boost.org/doc/libs/latest/doc/html/date_time/date_time_io.html#date_time.time_input_facet
    // Note that %Q is not supported for input, so we use the broader %ZP, which also handles simple offsets
    //TODO If boost ever starts using actual POSIX timezones instead of the inverse, the timezone offset here has to be reversed

    blt::local_date_time parsed(boost::date_time::not_a_date_time);
    {
      std::istringstream ss{std::string(str)};
      ss.exceptions(std::ios_base::badbit | std::ios_base::failbit);
      auto facet = new blt::local_time_input_facet("%Y-%m-%dT%H:%M:%S%F%ZP");
      ss.imbue(std::locale(ss.getloc(), facet));
      ss >> parsed;
      if (auto remaining = GetUnparsed(ss); !remaining.empty()) {
        throw std::invalid_argument(std::format("Unparsed data remains: {}", remaining));
      }
    }

    return TimestampFromBoostPtime(parsed.utc_time());
  }
};

class YyyyMmDdDateParser final : public TimestampParser {
public:
  explicit YyyyMmDdDateParser(std::string timeZone)
    : TimestampParser{"yyyymmdd"}, mTimeZone{std::move(timeZone)} {}

protected:
  Timestamp parseImpl(std::string_view str) override {
    // See https://www.boost.org/doc/libs/latest/doc/html/date_time/date_time_io.html#date_time.date_input_facet

    boost::gregorian::date parsedDate;
    {
      std::istringstream ss{std::string(str)};
      ss.exceptions(std::ios_base::badbit | std::ios_base::failbit);
      auto facet = new boost::gregorian::date_input_facet;
      facet->set_iso_format();
      ss.imbue(std::locale(ss.getloc(), facet));
      ss >> parsedDate;
      if (auto remaining = GetUnparsed(ss); !remaining.empty()) {
        throw std::invalid_argument(std::format("Unparsed data remains: {}", remaining));
      }
    }

    if (mTimeZone == SYSTEM_LOCAL_TIME_ZONE) {
      using Adjustor = boost::date_time::c_local_adjustor<bpt::ptime>;
      return TimestampFromBoostPtime(Adjustor::utc_to_local(bpt::ptime(parsedDate)));
    }

    const auto localTime = blt::local_date_time{
      parsedDate,
      bpt::time_duration{0, 0, 0},
      boost::make_shared<blt::posix_time_zone>(mTimeZone),
      blt::local_date_time::EXCEPTION_ON_ERROR};

    return TimestampFromBoostPtime(UtcTimeFromIncorrectPTime(localTime));
  }

private:
  /// Takes a Boost local_date_time object where the base_utc_offset is inverted
  /// and returns the UTC time as if the offset was set correctly.
  /// See https://github.com/boostorg/date_time/issues/240
  /// @note This function was added as a workaround for a bug in bpt::posix_time_zone, where
  ///   after parsing a posix timezone string, the base utc offset is set to the inverse of the expected value.
  static bpt::ptime UtcTimeFromIncorrectPTime(blt::local_date_time time) {
    // TODO: Remove this workaround when Boost has been fixed and then
    //  replace all calls to this function with a plain call to 'time.utc_time()',
    //  and then also see the TODO in XmlDateTimeParser.
    const auto offset = time.zone()->base_utc_offset();
    return time.utc_time() + offset + offset;
  }

  std::string mTimeZone;
};

} // namespace

TimeZone TimeZone::Local() { return {SYSTEM_LOCAL_TIME_ZONE}; }

bpt::ptime TimestampToBoostPtime(Timestamp time) {
  if (time == Timestamp::min()) { return boost::date_time::neg_infin; }
  if (time == Timestamp::max()) { return boost::date_time::pos_infin; }
  // Note: Range checking would be of limited use,
  //  because the STL already allows implicit conversion to higher-precision durations even if there could be data loss,
  //  e.g. sys_time<milliseconds>::max() can be converted to system_clock::time_point even if they are both int64_t and
  //  system_clock::time_point is in nanoseconds.
  //  Other things like dates also start to break down at large values, with years overflowing etc.
  return UnixEpochPtime + bpt::milliseconds{TicksSinceEpoch<milliseconds>(time)};
}

Timestamp TimestampFromBoostPtime(bpt::ptime ts) {
  if (ts.is_not_a_date_time()) { throw std::invalid_argument("Cannot convert not_a_date_time"); }
  // Only map infinities, not min & max Boost time, as there is no reason to use the latter when the former exist
  if (ts.is_neg_infinity()) return Timestamp::min();
  if (ts.is_pos_infinity()) return Timestamp::max();
  // See note about range checking above
  return Timestamp(milliseconds{(ts - UnixEpochPtime).total_milliseconds()});
}

std::string TimestampToXmlDateTime(Timestamp time) {
  std::ostringstream ss;
  ss.exceptions(std::ios_base::badbit | std::ios_base::failbit);
  auto facet = new bpt::time_facet("%Y-%m-%dT%H:%M:%S%FZ");
  ss.imbue(std::locale(ss.getloc(), facet));
  ss << TimestampToBoostPtime(time);
  return std::move(ss).str();
}

Timestamp TimestampFromXmlDataTime(std::string_view xml) {
  return XmlDateTimeParser{}.parse(xml);
}

Timestamp TimeZone::timestampFromYyyyMmDd(std::string_view yyyyMmDd) const {
  return YyyyMmDdDateParser{mStr}.parse(yyyyMmDd);
}


year_month_day BoostDateToStd(const boost::gregorian::date& date) {
  return year{date.year()} / month{date.month()} / day{date.day()};
}

boost::gregorian::date BoostDateFromStd(const year_month_day& date) {
  return {
    boost::numeric_cast<boost::gregorian::greg_year::value_type>(int{date.year()}),
    boost::numeric_cast<boost::gregorian::greg_month::value_type>(unsigned{date.month()}),
    boost::numeric_cast<boost::gregorian::greg_day::value_type>(unsigned{date.day()}),
  };
}

} // namespace pep
