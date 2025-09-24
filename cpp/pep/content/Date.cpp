#include <boost/algorithm/string/join.hpp>
#include <pep/content/Date.hpp>

#include <iomanip>
#include <vector>
#include <regex>

#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp> //include all types plus i/o

namespace pep {

namespace {

const std::array<std::string, 12> monthAbbrevs = {
  "jan",
  "feb",
  "mar",
  "apr",
  "may",
  "jun",
  "jul",
  "aug",
  "sep",
  "oct",
  "nov",
  "dec"
};

boost::date_time::months_of_year MonthIndexToBoost(int index) {
  if ((index < 0) || (index > 11)) {
    throw std::range_error("Month index must be between 0 and 11 (inclusive)");
  }

  //NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) XXX Check broken in older clang-tidy
  return boost::date_time::months_of_year(boost::date_time::months_of_year::Jan + index);
}

int MonthBoostToIndex(boost::date_time::months_of_year month) {
  if ((month < boost::date_time::months_of_year::Jan) || (month > boost::date_time::months_of_year::Dec)) {
    throw std::range_error("Month must be a value between January and December (inclusive)");
  }
  return month - boost::date_time::months_of_year::Jan;
}

const std::regex HOMEBREW_FORMAT_REGEX("^(0[1-9]|[12][0-9]|3[01])[-](" + boost::algorithm::join(monthAbbrevs, "|") + ")[-]([1-9][0-9]{3})$", std::regex_constants::icase);
const std::regex DDMMYYYY_FORMAT_REGEX(R"(^(0[1-9]|[12][0-9]|3[01])[-](0[1-9]|1[012])[-]([1-9][0-9]{3})$)");

std::optional<Date> TryParseDate(const std::regex& regex, const std::string& value, const std::function<boost::date_time::months_of_year(const std::string&)>& parseMonth) {
  std::smatch match;
  if (!std::regex_match(value, match, regex)) {
    return std::nullopt;
  }
  assert(match.size() == 4); // Full match plus 3 groups

  namespace bg = boost::gregorian;
  return Date(bg::date{
    boost::lexical_cast<bg::greg_year::value_type>(match[3].str()),
    parseMonth(match[2].str()),
    boost::lexical_cast<bg::greg_day::value_type>(match[1].str()),
  });
}

}


Date::Date(const boost::gregorian::date& gregorian)
  : mGregorian(gregorian) {
  if (mGregorian.is_special()) {
    throw std::runtime_error("Date can only be initialized with a concrete date value");
  }
}

std::optional<Date> Date::TryParseHomebrewFormat(const std::string& value) {
  return TryParseDate(HOMEBREW_FORMAT_REGEX, value, [](const std::string& enteredMonth) {
    auto lowercase = boost::algorithm::to_lower_copy(enteredMonth);
    auto begin = monthAbbrevs.begin(), end = monthAbbrevs.end();
    auto position = std::find(begin, end, lowercase);
    assert(position != end);
    auto iMonth = position - begin;
    return MonthIndexToBoost(static_cast<int>(iMonth));
    });
}

std::optional<Date> Date::TryParseDdMmYyyy(const std::string& date) {
  return TryParseDate(DDMMYYYY_FORMAT_REGEX, date, [](const std::string& enteredMonth) {
    auto number = stoi(enteredMonth);
    assert(number > 0);
    return MonthIndexToBoost(number - 1);
    });
}

std::string Date::toHomebrewFormat() const {
  auto day = mGregorian.day().as_number();
  auto iMonth = MonthBoostToIndex(mGregorian.month().as_enum());
  assert(iMonth >= 0);
  assert(iMonth < static_cast<int>(monthAbbrevs.size()));
  auto year = mGregorian.year();
  assert(year >= 1000);
  assert(year < 10000);

  std::stringstream result;
  result
    << std::setfill('0') << std::setw(2) << day << "-"
    << monthAbbrevs[static_cast<unsigned>(iMonth)] << "-"
    << year;
  return result.str();
}

DateTime::DateTime(time_t value)
  : mValue(value)
{}

DateTime DateTime::FromDeviceRecordTimestamp(int64_t timestamp) {
  return DateTime(time_t{timestamp / 1000});
}

int64_t DateTime::toDeviceRecordTimestamp() const {
  return int64_t{mValue * 1000};
}

}
