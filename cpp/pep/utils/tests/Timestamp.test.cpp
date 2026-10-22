#include <pep/utils/Timestamp.hpp>

#include <boost/date_time/posix_time/conversion.hpp>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <format>
#include <initializer_list>
#include <ctime>
#include <optional>
#include <string>

using namespace std::chrono;
using namespace std::literals;

namespace {

constexpr pep::Timestamp operator""_unixMs(const unsigned long long ms) noexcept {
  return pep::Timestamp(milliseconds{ms});
}

TEST(Timestamp, FromXmlDateTime) {
  constexpr auto xml = [](std::string_view str){return pep::TimeZone::Utc().timestampFromXmlDateTime(str);}; // function under test

  // UTC dates
  EXPECT_EQ(xml("2023-01-31T00:32:32+00:00"), 1675125152000_unixMs);
  EXPECT_EQ(xml("2023-01-31T00:32:32-00:00"), 1675125152000_unixMs);
  EXPECT_EQ(xml("2023-01-31T00:32:32Z"), 1675125152000_unixMs);
  EXPECT_EQ(xml("2024-02-29T13:00:00Z"), 1709211600000_unixMs) << "leap day should work";
  EXPECT_EQ(xml("1998-12-31T23:59:60Z"), 915148800000_unixMs) << "leap second should work";

  // Dates with (non-zero) UTC offset
  EXPECT_EQ(xml("2025-08-21T15:03:54+02:00"), 1755781434000_unixMs);

  // Date+times with fractional seconds
  EXPECT_EQ(xml("2023-01-31T00:32:32.42Z"), 1675125152420_unixMs) << "short fractional seconds should work";
  EXPECT_EQ(xml("2025-08-21T15:03:54.711354649+02:00"), 1755781434711_unixMs) << "long fractional seconds with timezone should work";

  // Bad dates: not following format
  EXPECT_THROW(xml(""), std::runtime_error);
  EXPECT_THROW(xml("2023-01-31 00:32:32"), std::runtime_error);
  EXPECT_THROW(xml("2026-10-155234345"), std::runtime_error);
  EXPECT_THROW(xml("31-01-2023T00:32:32Z"), std::runtime_error) << "dd-mm-yyyy should be rejected";
  EXPECT_THROW(xml("2026-10-15100:32:32Z"), std::runtime_error) << "wrong date-time separator should be rejected";
  EXPECT_THROW(xml("2026-10-15T00132:32Z"), std::runtime_error) << "wrong time separator should be rejected";

  // Non-existing dates
  EXPECT_THROW(xml("2027-11-00T00:00:00Z"), std::runtime_error) << "zero day should be rejected";
  EXPECT_THROW(xml("2027-11-32T00:00:00Z"), std::runtime_error) << "out-of-range day should be rejected";
  EXPECT_THROW(xml("2027-00-15T00:00:00Z"), std::runtime_error) << "zero month should be rejected";
  EXPECT_THROW(xml("2027-13-15T00:00:00Z"), std::runtime_error) << "out-of-range month should be rejected";
  EXPECT_THROW(xml("2023-01-31T25:00:00Z"), std::runtime_error) << "out-of-range hours should be rejected";
  EXPECT_THROW(xml("2023-01-31T00:60:00Z"), std::runtime_error) << "out-of-range minutes should be rejected";
  EXPECT_THROW(xml("2023-01-31T00:00:61Z"), std::runtime_error) << "out-of-range seconds should be rejected";
  EXPECT_THROW(xml("2027-02-29T00:00:00Z"), std::runtime_error); // feb 29, but not leap year
}

TEST(Timestamp, FromHttpDate) {
  EXPECT_EQ(pep::TimestampFromHttpDate("Sun, 06 Nov 1994 08:49:37 GMT"), 784111777000_unixMs);
  EXPECT_EQ(pep::TimestampFromHttpDate("Thu, 29 Feb 2024 13:00:00 GMT"), 1709211600000_unixMs); // leap day
  EXPECT_EQ(pep::TimestampFromHttpDate("Fri, 31 Dec 9999 23:59:59 GMT"), pep::Timestamp(sys_days{9999y / December / 31d}) + 23h + 59min + 59s);

  // Unsupported (obsolete) formats
  EXPECT_THROW((void) pep::TimestampFromHttpDate("Sunday, 06-Nov-94 08:49:37 GMT"), std::runtime_error); // RFC 850
  EXPECT_THROW((void) pep::TimestampFromHttpDate("Sun Nov  6 08:49:37 1994"), std::runtime_error); // asctime

  // Bad dates
  EXPECT_THROW((void) pep::TimestampFromHttpDate(""), std::runtime_error);
  EXPECT_THROW((void) pep::TimestampFromHttpDate("Sun, 06 Nov 1994 08:49:37"), std::runtime_error); // missing "GMT"
  EXPECT_THROW((void) pep::TimestampFromHttpDate("Sun, 06 Nov 1994 08:49:37 GMT trailing"), std::runtime_error);
  EXPECT_THROW((void) pep::TimestampFromHttpDate("Sun, 06 Foo 1994 08:49:37 GMT"), std::runtime_error);
  EXPECT_THROW((void) pep::TimestampFromHttpDate("Sat, 29 Feb 2025 00:00:00 GMT"), std::runtime_error); // feb 29, but not leap year
  EXPECT_THROW((void) pep::TimestampFromHttpDate("Sun, 06 Nov 1994 24:00:00 GMT"), std::runtime_error);
}

TEST(Timestamp, FromYyyyMmDd) {
  // function under test: timestampFromYyyyMmDd
  constexpr auto xml = [](std::string_view str){return pep::TimeZone::Utc().timestampFromXmlDateTime(str);}; // reference function
  const auto utc = pep::TimeZone::Utc();

  // Good dates - normal
  EXPECT_EQ(utc.timestampFromYyyyMmDd("19951205"), xml("1995-12-05T00:00:00Z"));
  EXPECT_EQ(utc.timestampFromYyyyMmDd("20230131"), xml("2023-01-31T00:00:00Z"));
  EXPECT_EQ(utc.timestampFromYyyyMmDd("20240229"), xml("2024-02-29T00:00:00Z")); // leap day

  // Good dates - edge cases
  EXPECT_EQ(utc.timestampFromYyyyMmDd("19700101"), xml("1970-01-01T00:00:00Z")); // epoch
  EXPECT_EQ(utc.timestampFromYyyyMmDd("99991231"), xml("9999-12-31T00:00:00Z")); // max yyyymmdd
}

// Check that timezones without DST are converted to UTC with the correct offset;
TEST(Timestamp, FromYyyyMmDd_SimpleTimezones) {
  // function under test: timestampFromYyyyMmDd
  constexpr auto xml = [](std::string_view str){return pep::TimeZone::Utc().timestampFromXmlDateTime(str);}; // reference function
  constexpr auto ptz = pep::TimeZone::PosixTimezone;

  EXPECT_EQ(ptz("MST7").timestampFromYyyyMmDd("20001002"), xml("2000-10-02T07:00:00Z")) << " UTC-7";
  EXPECT_EQ(ptz("GMT").timestampFromYyyyMmDd("20001002"), xml("2000-10-02T00:00:00Z")) << " UTC+0";
  EXPECT_EQ(ptz("MSK-3").timestampFromYyyyMmDd("20001002"), xml("2000-10-01T21:00:00Z")) << " UTC+3";
  EXPECT_EQ(ptz("IST-5:30").timestampFromYyyyMmDd("20001002"), xml("2000-10-01T18:30:00Z")) << " UTC+5:30";
  EXPECT_EQ(ptz("NPT-5:45").timestampFromYyyyMmDd("20001002"), xml("2000-10-01T18:15:00Z")) << " UTC+5:45";
  EXPECT_EQ(ptz("JST-9").timestampFromYyyyMmDd("20001002"), xml("2000-10-01T15:00:00Z")) << " UTC+9";

  // Edge case - push into leap day
  EXPECT_EQ(ptz("MSK-3").timestampFromYyyyMmDd("20040301"), xml("2004-02-29T21:00:00Z")) << " UTC+3 and date follows a leapday";

  // Edge case - date before Unix epoch
  EXPECT_EQ(ptz("JST-9").timestampFromYyyyMmDd("19700101"), xml("1969-12-31T15:00:00Z")) << "Date before Unix epoch";
}

TEST(Timestamp, FromYyyyMmDd_ComplexTimezones) {
  // function under test: timestampFromYyyyMmDd
  constexpr auto xml = [](std::string_view str){return pep::TimeZone::Utc().timestampFromXmlDateTime(str);}; // reference function
  const auto centralEuropeanTime = pep::TimeZone::PosixTimezone("CEST-1CET,M3.2.0/2:00:00,M11.1.0/2:00:00");
  const auto pacificTime = pep::TimeZone::PosixTimezone("PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00");

  EXPECT_EQ(centralEuropeanTime.timestampFromYyyyMmDd("20220115"), xml("2022-01-14T23:00:00Z")) << "UTC+1 (no DST)";
  EXPECT_EQ(centralEuropeanTime.timestampFromYyyyMmDd("20230505"), xml("2023-05-04T22:00:00Z")) << "UTC+2 (DST)";
  EXPECT_EQ(centralEuropeanTime.timestampFromYyyyMmDd("20241230"), xml("2024-12-29T23:00:00Z")) << "UTC+1 (no DST)";

  EXPECT_EQ(pacificTime.timestampFromYyyyMmDd("20241210"), xml("2024-12-10T08:00:00Z")) << "UTC-8 (no DST)";
  EXPECT_EQ(pacificTime.timestampFromYyyyMmDd("20240615"), xml("2024-06-15T07:00:00Z")) << "UTC-7 (DST)";

  // Edge case - push into leap day
  EXPECT_EQ(centralEuropeanTime.timestampFromYyyyMmDd("20280301"), xml("2028-02-29T23:00:00Z")) << "UTC+1 (no DST)";

  // Edge case - date before Unix epoch
  EXPECT_EQ(centralEuropeanTime.timestampFromYyyyMmDd("19700101"), xml("1969-12-31T23:00:00Z")) << "Date before Unix epoch";
}

// Check that timezones without DST are converted to UTC with the correct offset;
TEST(Timestamp, FromXmlDateTime_SimpleTimezones) {
  // function under test: timestampFromXmlDateTime
  constexpr auto utc = [](std::string_view str){return pep::TimeZone::Utc().timestampFromXmlDateTime(str);}; // reference function
  constexpr auto ptz = pep::TimeZone::PosixTimezone;

  EXPECT_EQ(ptz("MST7").timestampFromXmlDateTime("2000-10-02"), utc("2000-10-02T07:00:00Z")) << " UTC-7";
  EXPECT_EQ(ptz("GMT").timestampFromXmlDateTime("2000-10-02"), utc("2000-10-02T00:00:00Z")) << " UTC+0";
  EXPECT_EQ(ptz("MSK-3").timestampFromXmlDateTime("2000-10-02"), utc("2000-10-01T21:00:00Z")) << " UTC+3";
  EXPECT_EQ(ptz("IST-5:30").timestampFromXmlDateTime("2000-10-02"), utc("2000-10-01T18:30:00Z")) << " UTC+5:30";
  EXPECT_EQ(ptz("NPT-5:45").timestampFromXmlDateTime("2000-10-02"), utc("2000-10-01T18:15:00Z")) << " UTC+5:45";
  EXPECT_EQ(ptz("JST-9").timestampFromXmlDateTime("2000-10-02"), utc("2000-10-01T15:00:00Z")) << " UTC+9";

  // Edge case - push into leap day
  EXPECT_EQ(ptz("MSK-3").timestampFromXmlDateTime("2004-03-01"), utc("2004-02-29T21:00:00Z")) << " UTC+3 and date follows a leapday";

  // Edge case - date before Unix epoch
  EXPECT_EQ(ptz("JST-9").timestampFromXmlDateTime("1970-01-01"), utc("1969-12-31T15:00:00Z")) << "Date before Unix epoch";

  EXPECT_EQ(ptz("MST7").timestampFromXmlDateTime("2000-10-02T10:20:30"), utc("2000-10-02T17:20:30Z")) << " UTC-7, no timezone specified in date string";
  EXPECT_EQ(ptz("MST7").timestampFromXmlDateTime("2000-10-02T10:20:30Z"), utc("2000-10-02T10:20:30Z")) << " UTC-7, but UTC specified in date string";
  EXPECT_EQ(ptz("UTC").timestampFromXmlDateTime("2000-10-02T10:20:30+7:00"), utc("2000-10-02T03:20:30Z")) << " UTC, but UTC+7 specified in date string";
  EXPECT_EQ(ptz("MST7").timestampFromXmlDateTime("2000-10-02T10:20:30-3:00"), utc("2000-10-02T13:20:30Z")) << " UTC-7, but UTC-3 specified in date string";
}

TEST(Timestamp, LocalTimezone_MatchesMktime) {
  // Works with whatever the system's time zone is, so also on platforms where we can't set it (see below)
  const auto local = pep::TimeZone::Local();
  for (const auto& [year, month, day, hour] : std::initializer_list<std::array<int, 4>>{
         {2000, 10, 2, 0}, {2026, 1, 15, 12}, {2026, 7, 15, 12}, {2026, 10, 2, 23}}) {
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_isdst = -1;
    const auto expected = system_clock::from_time_t(std::mktime(&tm));
    const auto xml = std::format("{:04}-{:02}-{:02}T{:02}:00:00", year, month, day, hour);
    EXPECT_EQ(local.timestampFromXmlDateTime(xml), expected) << xml;
    if (hour == 0) {
      EXPECT_EQ(local.timestampFromYyyyMmDd(std::format("{:04}{:02}{:02}", year, month, day)), expected) << xml;
    }
  }
}

#ifndef __EMSCRIPTEN__ // Emscripten takes the local time zone from JavaScript and ignores TZ
/// Sets the system's local time zone (via the TZ environment variable) for the duration of its lifetime
class ScopedSystemTimeZone {
public:
  explicit ScopedSystemTimeZone(const char* tz) {
    //NOLINTNEXTLINE(concurrency-mt-unsafe) Tests are single-threaded
    if (const char* original = std::getenv("TZ")) { original_ = original; }
    set(tz);
  }
  ~ScopedSystemTimeZone() { set(original_ ? original_->c_str() : nullptr); }
  ScopedSystemTimeZone(const ScopedSystemTimeZone&) = delete;
  ScopedSystemTimeZone& operator=(const ScopedSystemTimeZone&) = delete;

private:
  static void set(const char* tz) {
#ifdef _WIN32
    _putenv_s("TZ", tz ? tz : "");
    _tzset();
#else
    //NOLINTBEGIN(concurrency-mt-unsafe) Tests are single-threaded
    if (tz) { setenv("TZ", tz, 1); }
    else { unsetenv("TZ"); }
    tzset();
    //NOLINTEND(concurrency-mt-unsafe)
#endif
  }

  std::optional<std::string> original_;
};

TEST(Timestamp, LocalTimezone) {
  constexpr auto utc = [](std::string_view str){return pep::TimeZone::Utc().timestampFromXmlDateTime(str);}; // reference function
  const auto local = pep::TimeZone::Local();

  {
    const ScopedSystemTimeZone tz("MSK-3"); // UTC+3
    EXPECT_EQ(local.timestampFromYyyyMmDd("20001002"), utc("2000-10-01T21:00:00Z"));
    EXPECT_EQ(local.timestampFromXmlDateTime("2000-10-02"), utc("2000-10-01T21:00:00Z"));
    EXPECT_EQ(local.timestampFromXmlDateTime("2000-10-02T10:20:30.5"), utc("2000-10-02T07:20:30.5Z"));
    EXPECT_EQ(local.timestampFromXmlDateTime("2000-10-02T10:20:30Z"), utc("2000-10-02T10:20:30Z")) << "Explicit zone should take precedence";
  }
  {
    const ScopedSystemTimeZone tz("MST7"); // UTC-7
    EXPECT_EQ(local.timestampFromYyyyMmDd("20001002"), utc("2000-10-02T07:00:00Z"));
    EXPECT_EQ(local.timestampFromXmlDateTime("2000-10-02T20:20:30"), utc("2000-10-03T03:20:30Z"));
  }
#ifndef _WIN32 // The Windows CRT does not support DST rules in TZ
  {
    const ScopedSystemTimeZone tz("CET-1CEST,M3.5.0,M10.5.0/3");
    EXPECT_EQ(local.timestampFromYyyyMmDd("20220115"), utc("2022-01-14T23:00:00Z")) << "UTC+1 (no DST)";
    EXPECT_EQ(local.timestampFromYyyyMmDd("20230505"), utc("2023-05-04T22:00:00Z")) << "UTC+2 (DST)";
    EXPECT_EQ(local.timestampFromXmlDateTime("2026-10-02T12:00:00"), utc("2026-10-02T10:00:00Z")) << "UTC+2 (DST)";
    // Around DST transitions: 2026-03-29 02:00 CET -> 03:00 CEST, 2026-10-25 03:00 CEST -> 02:00 CET
    EXPECT_EQ(local.timestampFromXmlDateTime("2026-03-29T01:30:00"), utc("2026-03-29T00:30:00Z")) << "Just before DST start";
    EXPECT_EQ(local.timestampFromXmlDateTime("2026-03-29T03:30:00"), utc("2026-03-29T01:30:00Z")) << "Just after DST start";
    EXPECT_EQ(local.timestampFromXmlDateTime("2026-10-25T01:30:00"), utc("2026-10-24T23:30:00Z")) << "Just before DST end";
    EXPECT_EQ(local.timestampFromXmlDateTime("2026-10-25T03:30:00"), utc("2026-10-25T02:30:00Z")) << "Just after DST end";
  }
#endif
}

#else // (!)__EMSCRIPTEN__

TEST(Timestamp, LocalTimezone) {
  GTEST_SKIP() << "Emscripten takes the local time zone from JavaScript and ignores TZ";
}

#endif // !__EMSCRIPTEN__

TEST(Timestamp, FromYyyyMmDd_TimezoneIndependentBehaviour) {
  using Timezone = pep::TimeZone;
  // function under test: timestampFromYyyyMmDd
  for (const auto& testParam :
       {Timezone::Utc(),
        Timezone::Local(), // can be included here, since the exact value should not affect the output
        Timezone::PosixTimezone("MSK-3")}) {

    // Bad dates - wrong input length
    EXPECT_THROW((void) testParam.timestampFromYyyyMmDd("2000101"), std::runtime_error);
    EXPECT_THROW((void) testParam.timestampFromYyyyMmDd("200111222"), std::runtime_error);

    // Non-existing dates
    EXPECT_THROW((void) testParam.timestampFromYyyyMmDd("20330022"), std::runtime_error);
    EXPECT_THROW((void) testParam.timestampFromYyyyMmDd("20331322"), std::runtime_error);
    EXPECT_THROW((void) testParam.timestampFromYyyyMmDd("20331132"), std::runtime_error);
    EXPECT_THROW((void) testParam.timestampFromYyyyMmDd("20331100"), std::runtime_error);
    EXPECT_THROW((void) testParam.timestampFromYyyyMmDd("20250229"), std::runtime_error) << "feb 29, but not leap year";
  }
}

TEST(Timestamp, ToXmlDateTime) {
  auto epoch = pep::Timestamp{/*zero*/};
  EXPECT_EQ(pep::TimestampToXmlDateTime(epoch), "1970-01-01T00:00:00Z");

  auto ts = pep::TimeZone::Utc().timestampFromXmlDateTime("2023-01-31T00:32:32+00:00");
  EXPECT_EQ(pep::TimestampToXmlDateTime(ts), "2023-01-31T00:32:32Z");
}

TEST(Timestamp, ToBoostPtime) {
  EXPECT_EQ(pep::TimestampToBoostPtime(pep::Timestamp::min()), boost::posix_time::ptime(boost::posix_time::neg_infin));
  EXPECT_EQ(pep::TimestampToBoostPtime(pep::Timestamp::max()), boost::posix_time::ptime(boost::posix_time::pos_infin));
  // Other cases already handled indirectly above
}

TEST(Timestamp, FromBoostPtime) {
  const boost::posix_time::ptime UnixEpoch(boost::gregorian::date(1970, 1, 1));
  EXPECT_EQ(UnixEpoch, boost::posix_time::from_time_t(0)); // Ensure that our constant in fact represents the start of the epoch
  EXPECT_EQ(pep::Timestamp{/*zero*/}, pep::TimestampFromBoostPtime(UnixEpoch));

  EXPECT_EQ(pep::TimestampFromBoostPtime(boost::posix_time::neg_infin), pep::Timestamp::min());
  EXPECT_EQ(pep::TimestampFromBoostPtime(boost::posix_time::pos_infin), pep::Timestamp::max());

  EXPECT_EQ(pep::TicksSinceEpoch<milliseconds>(
        pep::TimestampFromBoostPtime(boost::posix_time::ptime(boost::gregorian::date(1969, 12, 31)))),
      milliseconds{days{-1}}.count()) << "Time before Unix epoch should be handled";

  EXPECT_THROW((void) pep::TimestampFromBoostPtime(boost::posix_time::ptime(boost::posix_time::not_a_date_time)),
    std::invalid_argument);
}

TEST(BoostDate, ToStd) {
  using BoostMonth = boost::date_time::months_of_year;
  EXPECT_EQ(pep::BoostDateToStd(boost::gregorian::date(1912, BoostMonth::Jun, 23)), 1912y / June / 23d);
  EXPECT_EQ(pep::BoostDateToStd(boost::gregorian::date(2024, BoostMonth::Feb, 29)), 2024y / February / 29d)
      << "leap-day converted incorrectly";
  EXPECT_THROW((void) pep::BoostDateToStd(boost::gregorian::date(boost::gregorian::not_a_date_time)), std::logic_error)
      << "invalid Boost to std date conversion should fail";
}

TEST(BoostDate, FromStd) {
  using BoostMonth = boost::date_time::months_of_year;
  EXPECT_EQ(pep::BoostDateFromStd(1912y / June / 23d), boost::gregorian::date(1912, BoostMonth::Jun, 23));
  EXPECT_EQ(pep::BoostDateFromStd(2024y / February / 29d), boost::gregorian::date(2024, BoostMonth::Feb, 29))
      << "leap-day converted incorrectly";
  EXPECT_THROW((void) pep::BoostDateFromStd(0y / month{0} / 0d), std::logic_error)
      << "invalid std to Boost date conversion should fail";
}

}
