#include <gtest/gtest.h>
#include <chrono>
#include <boost/date_time/posix_time/conversion.hpp>
#include <pep/crypto/Timestamp.hpp>

namespace {

TEST(Timestamp, Now) {
  auto a = pep::Timestamp();
  auto b = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

  // Allow for a 1ms margin
  EXPECT_LE(std::abs(a.getTime() - b), 1);
}

TEST(Timestamp, XmlTimeZone) {
  std::string source;

  source = "2023-01-31T00:32:32Z"; // UTC shorthand
  EXPECT_EQ(pep::TryExtractXmlTimeZone(source)->count(), 0);
  EXPECT_EQ(source, "2023-01-31T00:32:32");

  source = "2023-01-31T00:32:32+00:00"; // UTC (positive zero offset)
  EXPECT_EQ(pep::TryExtractXmlTimeZone(source)->count(), 0);
  EXPECT_EQ(source, "2023-01-31T00:32:32");

  source = "2023-01-31T00:32:32-00:00"; // UTC (negative zero offset)
  EXPECT_EQ(pep::TryExtractXmlTimeZone(source)->count(), 0);
  EXPECT_EQ(source, "2023-01-31T00:32:32");

  source = "2025-08-21T15:03:54+02:00"; // 2 hours east of UTC, e.g. Amsterdam at DST
  EXPECT_EQ(pep::TryExtractXmlTimeZone(source)->count(), 120);
  EXPECT_EQ(source, "2025-08-21T15:03:54");

  source = "2025-08-21T15:03:54-09:30"; // 9h30m west of UTC (not a realistic example, but allowed by the format)
  EXPECT_EQ(pep::TryExtractXmlTimeZone(source)->count(), -570);
  EXPECT_EQ(source, "2025-08-21T15:03:54");

  source = "2023-01-31T00:32:32"; // No time zone specification
  EXPECT_EQ(std::nullopt, pep::TryExtractXmlTimeZone(source));

  source = "2023-01-31T00:32:32Z "; // Trailing space
  EXPECT_EQ(std::nullopt, pep::TryExtractXmlTimeZone(source));

  source = "2023-01-31T00:32:32-00:00 "; // Trailing space
  EXPECT_EQ(std::nullopt, pep::TryExtractXmlTimeZone(source));

  source = "2023-01-31T00:32:32+3:00 "; // Single-digit hour spec (no leading zero)
  EXPECT_EQ(std::nullopt, pep::TryExtractXmlTimeZone(source));

  source = "2023-01-31T00:32:32+00:60"; // 60 in the "mm" slot: should have been carried over as an (additional) hour into the "hh" slot
  EXPECT_EQ(std::nullopt, pep::TryExtractXmlTimeZone(source));

  source = "2023-01-31T00:32:32-00:73"; // More than 60 in the "mm" slot: should have been carried over as an (additional) hour into the "hh" slot
  EXPECT_EQ(std::nullopt, pep::TryExtractXmlTimeZone(source));
}

TEST(Timestamp, FromXmlDatetime) {
  const auto xml = pep::Timestamp::FromXmlDateTime; // function under test

  // UTC dates
  EXPECT_EQ(xml("2023-01-31T00:32:32+00:00").getTime(), 1675125152000);
  EXPECT_EQ(xml("2023-01-31T00:32:32-00:00").getTime(), 1675125152000);
  EXPECT_EQ(xml("2023-01-31T00:32:32Z").getTime(), 1675125152000);
  EXPECT_EQ(xml("2024-02-29T13:00:00Z").getTime(), 1709211600000); // leap day;

  // Dates with (non-zero) UTC offset
  EXPECT_EQ(xml("2025-08-21T15:03:54+02:00").getTime(), 1755781434000);

  // Date+times with fractional seconds
  EXPECT_EQ(xml("2025-08-21T15:03:54.711354649+02:00").getTime(), 1755781434711);

  // Bad dates: not following format
  EXPECT_THROW(xml(""), std::runtime_error);
  EXPECT_THROW(xml("2023-01-31 00:32:32"), std::runtime_error);
  EXPECT_THROW(xml("31-01-2023T00:32:32Z"), std::runtime_error);

  // Non-existing dates
  EXPECT_THROW(xml("2027-11-00T00:00:00Z"), std::runtime_error);
  EXPECT_THROW(xml("2027-11-32T00:00:00Z"), std::runtime_error);
  EXPECT_THROW(xml("2027-00-15T00:00:00Z"), std::runtime_error);
  EXPECT_THROW(xml("2027-13-15T00:00:00Z"), std::runtime_error);
  EXPECT_THROW(xml("2027-02-29T00:00:00Z"), std::runtime_error); // feb 29, but not leap year
}

TEST(Timestamp, FromIsoDate_Utc) {
  constexpr auto iso = pep::Timestamp::FromIsoDate; // function under test
  constexpr auto xml = pep::Timestamp::FromXmlDateTime; // reference function
  const auto utc = pep::Timestamp::TimeZone::Utc();

  // Good dates - normal
  EXPECT_EQ(iso("19951205", utc), xml("1995-12-05T00:00:00Z"));
  EXPECT_EQ(iso("20230131", utc), xml("2023-01-31T00:00:00Z"));
  EXPECT_EQ(iso("20240229", utc), xml("2024-02-29T00:00:00Z")); // leap day
 
  // Good dates - edge cases
  EXPECT_EQ(iso("19700101", utc), xml("1970-01-01T00:00:00Z")); // epoch
  EXPECT_EQ(iso("99991231", utc), xml("9999-12-31T00:00:00Z")); // max yyyymmdd
}

// Check that timezones without DST are converted to UTC with the correct offset;
TEST(Timestamp, FromIsoDate_SimpleTimezones) {
  constexpr auto iso = pep::Timestamp::FromIsoDate;     // function under test
  constexpr auto xml = pep::Timestamp::FromXmlDateTime; // reference function
  constexpr auto ptz = pep::Timestamp::TimeZone::PosixTimezone;

  EXPECT_EQ(iso("20001002", ptz("MST7")), xml("2000-10-02T07:00:00Z")) << " UTC-7";
  EXPECT_EQ(iso("20001002", ptz("GMT")), xml("2000-10-02T00:00:00Z")) << " UTC+0";
  EXPECT_EQ(iso("20001002", ptz("MSK-3")), xml("2000-10-01T21:00:00Z")) << " UTC+3";
  EXPECT_EQ(iso("20001002", ptz("IST-5:30")), xml("2000-10-01T18:30:00Z")) << " UTC+5:30";
  EXPECT_EQ(iso("20001002", ptz("NPT-5:45")), xml("2000-10-01T18:15:00Z")) << " UTC+5:45";
  EXPECT_EQ(iso("20001002", ptz("JST-9")), xml("2000-10-01T15:00:00Z")) << " UTC+9";

  // Edge case - push into leap day
  EXPECT_EQ(iso("20040301", ptz("MSK-3")), xml("2004-02-29T21:00:00Z")) << " UTC+3 and date follows a leapday";
 
  // Bad dates - push into unrepresentable
  EXPECT_THROW(iso("19700101", ptz("JST-9")), std::runtime_error) << " days before the epoch are not representable";
}

TEST(Timestamp, FromIsoDate_ComplexTimezones) {
  constexpr auto iso = pep::Timestamp::FromIsoDate;     // function under test
  constexpr auto xml = pep::Timestamp::FromXmlDateTime; // reference function
  const auto centralEuropeanTime = pep::Timestamp::TimeZone::PosixTimezone("CEST-1CET,M3.2.0/2:00:00,M11.1.0/2:00:00");
  const auto pacificTime = pep::Timestamp::TimeZone::PosixTimezone("PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00");

  EXPECT_EQ(iso("20220115", centralEuropeanTime), xml("2022-01-14T23:00:00Z")) << "UTC+1 (no DST)";
  EXPECT_EQ(iso("20230505", centralEuropeanTime), xml("2023-05-04T22:00:00Z")) << "UTC+2 (DST)";
  EXPECT_EQ(iso("20241230", centralEuropeanTime), xml("2024-12-29T23:00:00Z")) << "UTC+1 (no DST)";

  EXPECT_EQ(iso("20241210", pacificTime), xml("2024-12-10T08:00:00Z")) << "UTC-8 (no DST)";
  EXPECT_EQ(iso("20240615", pacificTime), xml("2024-06-15T07:00:00Z")) << "UTC-7 (DST)";

  // Edge case - push into leap day
  EXPECT_EQ(iso("20280301", centralEuropeanTime), xml("2028-02-29T23:00:00Z")) << "UTC+1 (no DST)";

  // Bad dates - push into unrepresentable
  EXPECT_THROW(iso("19700101", centralEuropeanTime), std::runtime_error) << "before epoch";
}

TEST(Timestamp, FromIsoDate_TimezoneIndependentBehaviour) {
  using Timezone = pep::Timestamp::TimeZone;
  constexpr auto iso = pep::Timestamp::FromIsoDate; // function under test
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

    // Unrepresentable dates - (assuming sensible timezone offsets < 24 hours)
    EXPECT_THROW(iso("18991231", testParam), std::runtime_error) << "before epoch";
  }
}

TEST(Timestamp, ToString) {
  auto epoch = pep::Timestamp(0);
  EXPECT_EQ(epoch.toString(), "1970-01-01T00:00:00Z");

  auto ts = pep::Timestamp::FromXmlDateTime("2023-01-31T00:32:32+00:00");
  EXPECT_EQ(ts.toString(), "2023-01-31T00:32:32Z");
}

TEST(Timestamp, Cmp) {
  auto a = pep::Timestamp::FromXmlDateTime("2023-01-31T00:32:32+00:00");
  auto b = pep::Timestamp::FromXmlDateTime("2023-01-31T00:32:33+00:00");

  EXPECT_GT(b, a);
  EXPECT_GE(b, a);
  EXPECT_LT(a, b);
  EXPECT_LE(a, b);

  auto c = pep::Timestamp::FromXmlDateTime("2023-01-31T00:32:33+00:00");
  EXPECT_EQ(b, c);
  EXPECT_GE(b, c);
  EXPECT_LE(b, c);
}

TEST(Timestamp, Min) {
  auto a = pep::Timestamp::Min();
  EXPECT_EQ(a.getTime(), 0);
  EXPECT_LT(a.getTime(), pep::Timestamp().getTime());
  EXPECT_EQ(a.toString(), "1970-01-01T00:00:00Z");
}

TEST(Timestamp, Max) {
  auto a = pep::Timestamp::Max();
  EXPECT_GT(a.getTime(), pep::Timestamp().getTime());
}

TEST(Timestamp, ToTimeT) {
  EXPECT_EQ(pep::Timestamp(0).toTime_t(), time_t{0});
  EXPECT_EQ(pep::Timestamp(1000).toTime_t(), time_t{1});
}

TEST(Timestamp, FromTimeT) {
  EXPECT_EQ(pep::Timestamp(0), pep::Timestamp::FromTimeT(time_t{0}));
  EXPECT_EQ(pep::Timestamp(1000), pep::Timestamp::FromTimeT(time_t{1}));
}

TEST(Timestamp, FromPtime) {
  const boost::posix_time::ptime EPOCH(boost::gregorian::date(1970, 1, 1));
  EXPECT_EQ(EPOCH, boost::posix_time::from_time_t(0)); // Ensure that our constant in fact represents the start of the epoch
  EXPECT_EQ(pep::Timestamp(0), pep::Timestamp::FromPtime(EPOCH));

  EXPECT_ANY_THROW(pep::Timestamp::FromPtime(boost::posix_time::ptime(boost::posix_time::not_a_date_time)));  // Sentinel (not-a-datetime) value
  EXPECT_ANY_THROW(pep::Timestamp::FromPtime(boost::posix_time::ptime(boost::gregorian::date(1969, 7, 16)))); // Before Unix epoch
}

}
