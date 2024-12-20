#include <pep/crypto/Timestamp.hpp>

#include <chrono>
#include <ctime>
#include <pep/utils/Platform.hpp>
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
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/time_parsers.hpp>
#include <boost/date_time/time_clock.hpp>
#include <boost/date_time/time_duration.hpp>
#include <boost/exception/all.hpp>
#include <boost/format.hpp>
#include <boost/smart_ptr/make_shared_array.hpp>

namespace pep {

namespace {

constexpr auto XML_DATE_TIME_FORMAT = "%04d-%02d-%02dT%02d:%02d:%02dZ";

constexpr auto SYSTEM_LOCAL_TIME_ZONE = "SYSTEM_TIME_ZONE";

/// Converts strings to pep::TimestampValues
class TimestampParser {
public:
  virtual ~TimestampParser() = default;

  /// Parses the input that was passed at construction.
  pep::Timestamp parse() {
    checkInput(mRawInput);
    const auto output = preprocessedInputToTimeT(preprocessInput(mRawInput));
    checkRawOutput(output);
    return pep::Timestamp::FromTimeT(output);
  }

protected:
  static constexpr auto E_MSG_NOT_MATCHING_FORMAT = "input does not match format specification";
  static constexpr auto E_MSG_UNREPRESENTABLE_VALUE = "unrepresentable value";

  TimestampParser(std::string name, std::string initialInput)
    : mFormatName{std::move(name)}, mRawInput{std::move(initialInput)} {}

  std::runtime_error parsingError(const std::string& details) const {
    return std::runtime_error{"Couldn't parse \"" + mRawInput + "\" as " + mFormatName + ": " + details};
  }

  virtual void checkInput(const std::string&) const = 0;         ///< lexical checks on the input string
  virtual std::string preprocessInput(std::string) = 0;          ///< string operations before parsing
  virtual std::time_t preprocessedInputToTimeT(std::string) = 0; ///< actual parsing of the string

private:
  void checkRawOutput(std::time_t time) {
    const auto inRepresentableRange = Timestamp::min().toTime_t() <= time && time <= Timestamp::max().toTime_t();
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
        && str.find(' ') == std::string::npos // no space chars before we swap out 'T'
        && (str.back() == 'Z' || str.ends_with("+00:00") || str.ends_with("-00:00")); // accepted endings
    if (!formatOk) { throw parsingError(E_MSG_NOT_MATCHING_FORMAT); }
  }

  std::string preprocessInput(std::string str) override {
    const auto sizeWithoutTimezone = str.size() - (str.back() == 'Z' ? std::size_t{1} : std::size_t{6});
    str.resize(sizeWithoutTimezone);
    std::ranges::replace(str, 'T', ' ');
    return str;
  }

  std::time_t preprocessedInputToTimeT(std::string str) override {
    try {
      const auto ptime = boost::posix_time::time_from_string(str);
      return boost::posix_time::to_time_t(ptime);
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

  std::string preprocessInput(std::string str) override { return str; }

  std::time_t preprocessedInputToTimeT(std::string str) override {
    try {
      return boost::posix_time::to_time_t(InterpretDateWithBoost(str, mTimeZone));
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

    const auto parsedTimeZone = boost::make_shared<blt::posix_time_zone>(posixTimeZone);
    const auto localTime = blt::local_date_time{
        parsedDate,
        bpt::time_duration{0, 0, 0},
        parsedTimeZone,
        blt::local_date_time::EXCEPTION_ON_ERROR};

    return UtcTimeFromIncorrectPTime(localTime);
  }

  std::string mTimeZone;
};

} // namespace

Timestamp::Timestamp() {
  using namespace std::chrono;
  mValue = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

Timestamp::TimeZone Timestamp::TimeZone::Local() { return {SYSTEM_LOCAL_TIME_ZONE}; }

time_t Timestamp::toTime_t() const { return mValue / 1000; }

Timestamp Timestamp::FromTimeT(time_t ts) { return Timestamp(ts * 1000); }

std::string Timestamp::toString() const {
  time_t ts = this->toTime_t();
  struct tm tm;
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

} // namespace pep
