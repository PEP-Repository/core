#include <gtest/gtest.h>
#include <chrono>
#include <boost/date_time/posix_time/conversion.hpp>
#include <pep/crypto/Timestamp.hpp>

using namespace std::chrono;
using namespace std::literals;

namespace {

constexpr pep::Timestamp operator""_unixMs(const unsigned long long ms) noexcept {
  return pep::Timestamp(milliseconds{ms});
}

TEST(Timestamp, from_xml_date_time) {
  const auto xml = pep::Timestamp::from_xml_date_time; // function under test

  // UTC dates
  EXPECT_EQ(xml("2023-01-31T00:32:32+00:00"), 1675125152000_unixMs);
  EXPECT_EQ(xml("2023-01-31T00:32:32-00:00"), 1675125152000_unixMs);
  EXPECT_EQ(xml("2023-01-31T00:32:32Z"), 1675125152000_unixMs);
  EXPECT_EQ(xml("2024-02-29T13:00:00Z"), 1709211600000_unixMs); // leap day;

  // Dates with (non-zero) UTC offset
  EXPECT_EQ(xml("2025-08-21T15:03:54+02:00"), 1755781434000_unixMs);

  // Date+times with fractional seconds
  EXPECT_EQ(xml("2025-08-21T15:03:54.711354649+02:00"), 1755781434711_unixMs);

  // Bad dates: not following format
  EXPECT_THROW(xml(""), std::runtime_error);
  //EXPECT_THROW(xml("2023-01-31 00:32:32"), std::runtime_error);
  EXPECT_THROW(xml("31-01-2023T00:32:32Z"), std::runtime_error);

  // Non-existing dates
  EXPECT_THROW(xml("2027-11-00T00:00:00Z"), std::runtime_error);
  EXPECT_THROW(xml("2027-11-32T00:00:00Z"), std::runtime_error);
  EXPECT_THROW(xml("2027-00-15T00:00:00Z"), std::runtime_error);
  EXPECT_THROW(xml("2027-13-15T00:00:00Z"), std::runtime_error);
  EXPECT_THROW(xml("2027-02-29T00:00:00Z"), std::runtime_error); // feb 29, but not leap year
}

TEST(Timestamp, from_yyyymmdd_Utc) {
  constexpr auto iso = pep::Timestamp::from_yyyymmdd; // function under test
  constexpr auto xml = pep::Timestamp::from_xml_date_time; // reference function
  const auto utc = pep::TimeZone::Utc();

  // Good dates - normal
  EXPECT_EQ(iso("19951205", utc), xml("1995-12-05T00:00:00Z"));
  EXPECT_EQ(iso("20230131", utc), xml("2023-01-31T00:00:00Z"));
  EXPECT_EQ(iso("20240229", utc), xml("2024-02-29T00:00:00Z")); // leap day

  // Good dates - edge cases
  EXPECT_EQ(iso("19700101", utc), xml("1970-01-01T00:00:00Z")); // epoch
  EXPECT_EQ(iso("99991231", utc), xml("9999-12-31T00:00:00Z")); // max yyyymmdd
}

// Check that timezones without DST are converted to UTC with the correct offset;
TEST(Timestamp, from_yyyymmdd_SimpleTimezones) {
  constexpr auto iso = pep::Timestamp::from_yyyymmdd;     // function under test
  constexpr auto xml = pep::Timestamp::from_xml_date_time; // reference function
  constexpr auto ptz = pep::TimeZone::PosixTimezone;

  EXPECT_EQ(iso("20001002", ptz("MST7")), xml("2000-10-02T07:00:00Z")) << " UTC-7";
  EXPECT_EQ(iso("20001002", ptz("GMT")), xml("2000-10-02T00:00:00Z")) << " UTC+0";
  EXPECT_EQ(iso("20001002", ptz("MSK-3")), xml("2000-10-01T21:00:00Z")) << " UTC+3";
  EXPECT_EQ(iso("20001002", ptz("IST-5:30")), xml("2000-10-01T18:30:00Z")) << " UTC+5:30";
  EXPECT_EQ(iso("20001002", ptz("NPT-5:45")), xml("2000-10-01T18:15:00Z")) << " UTC+5:45";
  EXPECT_EQ(iso("20001002", ptz("JST-9")), xml("2000-10-01T15:00:00Z")) << " UTC+9";

  // Edge case - push into leap day
  EXPECT_EQ(iso("20040301", ptz("MSK-3")), xml("2004-02-29T21:00:00Z")) << " UTC+3 and date follows a leapday";

  // Edge case - date before Unix epoch
  EXPECT_EQ(iso("19700101", ptz("JST-9")), xml("1969-12-31T15:00:00Z")) << "Date before Unix epoch";
}

TEST(Timestamp, from_yyyymmdd_ComplexTimezones) {
  constexpr auto iso = pep::Timestamp::from_yyyymmdd;     // function under test
  constexpr auto xml = pep::Timestamp::from_xml_date_time; // reference function
  const auto centralEuropeanTime = pep::TimeZone::PosixTimezone("CEST-1CET,M3.2.0/2:00:00,M11.1.0/2:00:00");
  const auto pacificTime = pep::TimeZone::PosixTimezone("PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00");

  EXPECT_EQ(iso("20220115", centralEuropeanTime), xml("2022-01-14T23:00:00Z")) << "UTC+1 (no DST)";
  EXPECT_EQ(iso("20230505", centralEuropeanTime), xml("2023-05-04T22:00:00Z")) << "UTC+2 (DST)";
  EXPECT_EQ(iso("20241230", centralEuropeanTime), xml("2024-12-29T23:00:00Z")) << "UTC+1 (no DST)";

  EXPECT_EQ(iso("20241210", pacificTime), xml("2024-12-10T08:00:00Z")) << "UTC-8 (no DST)";
  EXPECT_EQ(iso("20240615", pacificTime), xml("2024-06-15T07:00:00Z")) << "UTC-7 (DST)";

  // Edge case - push into leap day
  EXPECT_EQ(iso("20280301", centralEuropeanTime), xml("2028-02-29T23:00:00Z")) << "UTC+1 (no DST)";

  // Edge case - date before Unix epoch
  EXPECT_EQ(iso("19700101", centralEuropeanTime), xml("1969-12-31T23:00:00Z")) << "Date before Unix epoch";
}

TEST(Timestamp, from_yyyymmdd_TimezoneIndependentBehaviour) {
  using Timezone = pep::TimeZone;
  constexpr auto iso = pep::Timestamp::from_yyyymmdd; // function under test
  for (const auto& testParam :
       {Timezone::Utc(),
        Timezone::Local(), // can be included here, since the exact value should not affect the output
        Timezone::PosixTimezone("MSK-3")}) {

    // Bad dates - wrong input length
    EXPECT_THROW(iso("2000101", testParam), std::runtime_error);
    EXPECT_THROW(iso("200111222", testParam), std::runtime_error);

    // Non-existing dates
    EXPECT_THROW(iso("20330022", testParam), std::runtime_error);
    EXPECT_THROW(iso("20331322", testParam), std::runtime_error);
    EXPECT_THROW(iso("20331132", testParam), std::runtime_error);
    EXPECT_THROW(iso("20331100", testParam), std::runtime_error);
    EXPECT_THROW(iso("20250229", testParam), std::runtime_error) << "feb 29, but not leap year";
  }
}

TEST(Timestamp, to_xml_date_time) {
  auto epoch = pep::Timestamp::zero();
  EXPECT_EQ(epoch.to_xml_date_time(), "1970-01-01T00:00:00Z");

  auto ts = pep::Timestamp::from_xml_date_time("2023-01-31T00:32:32+00:00");
  EXPECT_EQ(ts.to_xml_date_time(), "2023-01-31T00:32:32Z");
}

TEST(Timestamp, to_time_t) {
  EXPECT_EQ(pep::Timestamp::zero().to_time_t(), time_t{0});
  EXPECT_EQ(1000_unixMs .to_time_t(), time_t{1});
}

TEST(Timestamp, from_time_t) {
  EXPECT_EQ(pep::Timestamp::zero(), pep::Timestamp::from_time_t(time_t{0}));
  EXPECT_EQ(1000_unixMs, pep::Timestamp::from_time_t(time_t{1}));
}

TEST(Timestamp, to_boost_ptime) {
  EXPECT_EQ(pep::Timestamp(pep::Timestamp::min()).to_boost_ptime(), boost::posix_time::ptime(boost::posix_time::neg_infin));
  EXPECT_EQ(pep::Timestamp(pep::Timestamp::max()).to_boost_ptime(), boost::posix_time::ptime(boost::posix_time::pos_infin));
  // Other cases already handled indirectly above
}

TEST(Timestamp, from_boost_ptime) {
  const boost::posix_time::ptime UNIX_EPOCH(boost::gregorian::date(1970, 1, 1));
  EXPECT_EQ(UNIX_EPOCH, boost::posix_time::from_time_t(0)); // Ensure that our constant in fact represents the start of the epoch
  EXPECT_EQ(pep::Timestamp::zero(), pep::Timestamp::from_boost_ptime(UNIX_EPOCH));

  EXPECT_EQ(pep::Timestamp::from_boost_ptime(boost::posix_time::neg_infin), pep::Timestamp::min());
  EXPECT_EQ(pep::Timestamp::from_boost_ptime(boost::posix_time::pos_infin), pep::Timestamp::max());

  EXPECT_EQ(pep::Timestamp::from_boost_ptime(boost::posix_time::ptime(boost::gregorian::date(1969, 12, 31)))
    .ticks_since_epoch<milliseconds>(),
    milliseconds{days{-1}}.count()) << "Time before Unix epoch should be handled";

  EXPECT_THROW((void) pep::Timestamp::from_boost_ptime(boost::posix_time::ptime(boost::posix_time::not_a_date_time)),
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
