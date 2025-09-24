#include <pep/crypto/Timestamp.hpp>

#include <chrono>
#include <ctime>
#include <pep/utils/Platform.hpp>
#include <regex>
#include <stdexcept>

#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/date_parsing.hpp>
#include <boost/date_time/gregorian/greg_month.hpp>
#include <boost/date_time/gregorian/parsers.hpp>
#include <boost/date_time/gregorian_calendar.hpp>
#include <boost/date_time/local_time/local_date_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/local_time/local_time_types.hpp>
#include <boost/date_time/local_time/posix_time_zone.hpp>
#include <boost/date_time/posix_time/conversion.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_config.hpp>
#include <boost/date_time/posix_time/posix_time_system.hpp>
#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/time_clock.hpp>
#include <boost/date_time/time_duration.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <boost/format.hpp>
#include <boost/make_shared.hpp>

namespace pep {

namespace {

constexpr auto XML_DATE_TIME_FORMAT = "%04d-%02d-%02dT%02d:%02d:%02dZ";

constexpr auto SYSTEM_LOCAL_TIME_ZONE = "SYSTEM_TIME_ZONE";

constexpr auto EPOCH_OFFSET = boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1));

/// Converts strings to pep::Timestamp values
class TimestampParser {
public:
  virtual ~TimestampParser() = default;

  /// Parses the input that was passed at construction.
  pep::Timestamp parse() {
    checkInput(mRawInput);
    const auto output = inputToPtime(mRawInput);
    checkRawOutput(output);
    return pep::Timestamp::FromPtime(output);
  }

protected:
  static constexpr auto E_MSG_NOT_MATCHING_FORMAT = "input does not match format specification";
  static constexpr auto E_MSG_UNREPRESENTABLE_VALUE = "unrepresentable value";

  TimestampParser(std::string name, std::string initialInput)
    : mFormatName{std::move(name)}, mRawInput{std::move(initialInput)} {}

  std::runtime_error parsingError(const std::string& details) const {
    return std::runtime_error{"Couldn't parse \"" + mRawInput + "\" as " + mFormatName + ": " + details};
  }

  virtual void checkInput(const std::string&) const = 0;          ///< lexical checks on the input string
  virtual boost::posix_time::ptime inputToPtime(std::string) = 0; ///< actual parsing of the string

private:
  void checkRawOutput(boost::posix_time::ptime time) {
    const auto min = Timestamp::Min().toPtime(), max = Timestamp::Max().toPtime();
    const auto inRepresentableRange = min <= time && time <= max;
    if (!inRepresentableRange) { throw parsingError(E_MSG_UNREPRESENTABLE_VALUE); }
  }

  std::string mFormatName;
  std::string mRawInput;
};

class XmlDateTimeParser : public TimestampParser {
public:
  XmlDateTimeParser(std::string input) : TimestampParser{"xml date-time", std::move(input)} {}

  void checkInput(const std::string& str) const override {
    const auto formatOk = !str.empty() // so we can safely call str.back() and similar operations from this point onward
        && std::ranges::count(str, 'T') == 1 // requirement for .normalizedInputToTimeT
        && str.find(' ') == std::string::npos; // no space chars before we swap out 'T'
    if (!formatOk) { throw parsingError(E_MSG_NOT_MATCHING_FORMAT); }
  }

  boost::posix_time::ptime inputToPtime(std::string str) override {
    try {
      // Extract the time zone specification if present
      auto offset = TryExtractXmlTimeZone(str);

      // Replace XML's 'T' delimiter by the space that Boost's parsing function accepts
      std::ranges::replace(str, 'T', ' ');
      auto ptime = boost::posix_time::time_from_string(str);

      // Apply time zone (offset) to result
      if (offset.has_value()) {
        ptime -= boost::posix_time::minutes(offset->count());
      }
      return ptime;
    }
    catch (const boost::exception& e) {
      throw parsingError(boost::diagnostic_information(e));
    }
  }
};

class IsoDateParser final : public TimestampParser {
public:
  IsoDateParser(std::string input, std::string timeZone)
    : TimestampParser{"iso date (yyyymmdd)", std::move(input)}, mTimeZone{std::move(timeZone)} {}

protected:
  void checkInput(const std::string& str) const override {
    if (str.length() != 8 || !std::all_of(str.begin(), str.end(), ::isdigit)) {
      throw parsingError(E_MSG_NOT_MATCHING_FORMAT);
    }
  }

  boost::posix_time::ptime inputToPtime(std::string str) override {
    try {
      return InterpretDateWithBoost(str, mTimeZone);
    }
    catch (const boost::exception& e) {
      throw parsingError(boost::diagnostic_information(e));
    }
  }

private:
  /// Takes a Boost local_date_time object where the base_utc_offset is inverted
  /// and returns the UTC time as if the offset was set correctly.
  ///
  /// @note This function was added as a workaround for a bug in boost::posix_time::posix_time_zone, where
  ///   after parsing a posix timezone string, the base utc offset is set to the inverse of the expected value.
  boost::posix_time::ptime UtcTimeFromIncorrectPTime(boost::local_time::local_date_time time) {
    // TODO: Remove this workaround when Boost has been fixed and then
    // replace all calls to this function with a plain call to 'time.utc_time()'.
    const auto offset = time.zone()->base_utc_offset();
    return time.utc_time() + offset + offset;
  }

  boost::posix_time::ptime InterpretDateWithBoost(const std::string& isoDate, const std::string& posixTimeZone) {
    namespace bpt = boost::posix_time;
    namespace blt = boost::local_time;

    const auto parsedDate = boost::gregorian::from_undelimited_string(isoDate);
    const auto parsedDateTime = bpt::ptime{parsedDate};
    if (posixTimeZone == SYSTEM_LOCAL_TIME_ZONE) {
      using Adjustor = boost::date_time::c_local_adjustor<bpt::ptime>;
      return Adjustor::utc_to_local(parsedDateTime);
    }

    const auto localTime = blt::local_date_time{
        parsedDate,
        bpt::time_duration{0, 0, 0},
        boost::make_shared<blt::posix_time_zone>(posixTimeZone),
        blt::local_date_time::EXCEPTION_ON_ERROR};

    return UtcTimeFromIncorrectPTime(localTime);
  }

  std::string mTimeZone;
};

} // namespace

Timestamp Timestamp::Max() {
  // Ensure that Timestamp::Max() can be converted to a valid boost::posix::ptime
  auto ptime_max_msec = (boost::posix_time::ptime(boost::posix_time::special_values::max_date_time) - EPOCH_OFFSET).total_milliseconds();
  auto msec = std::min(std::numeric_limits<int64_t>::max(), ptime_max_msec);
  return Timestamp(msec);
}

Timestamp::Timestamp() {
  using namespace std::chrono;
  mValue = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

Timestamp::TimeZone Timestamp::TimeZone::Local() { return {SYSTEM_LOCAL_TIME_ZONE}; }

time_t Timestamp::toTime_t() const { return mValue / 1000; }

boost::posix_time::ptime Timestamp::toPtime() const {
  return boost::posix_time::ptime(EPOCH_OFFSET + boost::posix_time::milliseconds(mValue));
}

Timestamp Timestamp::FromTimeT(time_t ts) { return Timestamp(ts * 1000); }

Timestamp Timestamp::FromPtime(boost::posix_time::ptime ts) {
  if (ts == boost::posix_time::not_a_date_time) {
    throw std::runtime_error("Can't construct a Timestamp from an invalid (sentinel) ptime");
  }
  if (ts < EPOCH_OFFSET) {
    throw std::runtime_error("Can't construct a Timestamp outside the Unix epoch");
  }
  return Timestamp((ts - EPOCH_OFFSET).total_milliseconds());
}

std::string Timestamp::toString() const {
  time_t ts = this->toTime_t();
  std::tm tm{};
  if (!gmtime_r(&ts, &tm)) {
    throw std::runtime_error("Failed to convert time");
  }

  return (boost::format(XML_DATE_TIME_FORMAT)
      % (tm.tm_year + 1900) % (tm.tm_mon + 1) % tm.tm_mday
      % tm.tm_hour % tm.tm_min % tm.tm_sec).str();
}

Timestamp Timestamp::FromXmlDateTime(const std::string& xml) {
  return XmlDateTimeParser{xml}.parse();
}

Timestamp Timestamp::FromIsoDate(const std::string& yyyymmdd, TimeZone timeZone) {
  return IsoDateParser{yyyymmdd, timeZone.mStr}.parse();
}

std::optional<std::chrono::minutes> TryExtractXmlTimeZone(std::string& source) {
  if (source.empty()) {
    return std::nullopt;
  }

  // Cheap handler for 'Z' suffix, which indicates UTC
  if (source.back() == 'Z') {
    source.resize(source.size() - 1U);
    return std::chrono::minutes(0);
  }

  // Regex handler for "shh:mm", where 's' is the '+' or '-' sign 
  static constexpr const char* TIME_ZONE_REGEX_EXPR = "([+-])(\\d{2}):(\\d{2})$";
  constexpr size_t TIME_ZONE_SPEC_LENGTH = 6U;

  std::regex ex(TIME_ZONE_REGEX_EXPR);
  std::smatch matches;
  if (!std::regex_search(source, matches, ex)) {
    return std::nullopt;
  }

  assert(matches.size() == 4U); // The entire expression plus its three groups
  assert(std::ranges::all_of(matches, [](const auto& single) { return single.matched; }));
  assert(matches[0].str().size() == TIME_ZONE_SPEC_LENGTH); // Entire time zone spec "shh:mm"
  assert(matches[1].str().size() == 1U); // Sign: either '+' or '-'
  assert(matches[2].str().size() == 2U); // hh
  assert(matches[3].str().size() == 2U); // mm

  auto hours = std::stol(matches[2].str());
  auto minutes = std::stol(matches[3].str());
  if (minutes >= 60) {
    return std::nullopt; // Should have been carried over as an additional hour into the "hh" slot
  }

  auto result = std::chrono::hours(hours) + std::chrono::minutes(minutes);
  if (matches[1].str().front() == '-') {
    result = -result;
  }

  source.resize(source.size() - TIME_ZONE_SPEC_LENGTH);
  return std::chrono::duration_cast<std::chrono::minutes>(result);
}

} // namespace pep
