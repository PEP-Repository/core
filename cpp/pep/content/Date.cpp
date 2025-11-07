#include <pep/content/Date.hpp>

#include <iomanip>
#include <regex>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/lexical_cast.hpp>

using namespace std::chrono;

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

const std::regex DdMonthAbbrevYyyyRegEx("^(0[1-9]|[12][0-9]|3[01])[-](" + boost::algorithm::join(monthAbbrevs, "|") + ")[-]([1-9][0-9]{3})$", std::regex_constants::icase);
const std::regex DdMmYyyyRegEx(R"(^(0[1-9]|[12][0-9]|3[01])[-](0[1-9]|1[012])[-]([1-9][0-9]{3})$)");

std::optional<year_month_day> TryParseDate(const std::regex& regex, const std::string& value, const std::function<month(const std::string&)>& parseMonth) {
  std::smatch match;
  if (!std::regex_match(value, match, regex)) {
    return std::nullopt;
  }
  assert(match.size() == 4 && "Invalid date RegEx"); // Full match plus 3 groups

  return year{boost::lexical_cast<int>(match[3])}
      / parseMonth(match[2].str())
      / day{boost::lexical_cast<unsigned>(match[1].str())};
}

}

std::optional<year_month_day> TryParseDdMonthAbbrevYyyyDate(const std::string& value) {
  return TryParseDate(DdMonthAbbrevYyyyRegEx, value, [](const std::string& enteredMonth) {
    auto lowercase = boost::algorithm::to_lower_copy(enteredMonth);
    auto begin = monthAbbrevs.begin(), end = monthAbbrevs.end();
    auto position = std::find(begin, end, lowercase);
    assert(position != end && "RegEx matched invalid month");
    return January + months{position - begin};
    });
}

std::optional<year_month_day> TryParseDdMmYyyy(const std::string& date) {
  return TryParseDate(DdMmYyyyRegEx, date, [](const std::string& enteredMonth) {
    return month{boost::lexical_cast<unsigned>(enteredMonth)};
    });
}

std::string ToDdMonthAbbrevYyyyDate(const year_month_day& date) {
  auto day = unsigned{date.day()};
  auto iMonth = unsigned{date.month()};
  auto year = int{date.year()};
  if (year < 0) {
    throw std::range_error("year cannot be negative for this date format");
  }

  std::stringstream result;
  result
    << std::setfill('0') << std::setw(2) << day << "-"
    << monthAbbrevs.at(iMonth) << "-"
    << std::setfill('0') << std::setw(4) << year;
  return std::move(result).str();
}

}
