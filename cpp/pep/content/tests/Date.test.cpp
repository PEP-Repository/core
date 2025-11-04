#include <pep/content/Date.hpp>
#include <pep/content/ParticipantPersonalia.hpp>
#include <pep/utils/MiscUtil.hpp>

#include <gtest/gtest.h>
#include <exception>

#include <boost/date_time/gregorian/greg_date.hpp>

using namespace std::literals;
using namespace std::chrono;

namespace {

const auto DateNotOk = std::not_fn(pep::MethodAsFree(&year_month_day::ok));

TEST(Date, MultiFormat) {
  using PP = pep::ParticipantPersonalia; // class under test

  auto dateWithMonthAbbrev = PP::ParseDateOfBirth("23-jun-1912");
  EXPECT_EQ(year_month_day{dateWithMonthAbbrev}, 1912y / June / 23d) << "date with month abbreviation parsed incorrectly";

  auto ddmmyyyy = PP::ParseDateOfBirth("28-12-1903");
  EXPECT_EQ(year_month_day{ddmmyyyy}, 1903y / December / 28d) << "DD-MM-YYYY date parsed incorrectly";

  // edge cases
  auto leap_day = PP::ParseDateOfBirth("29-feb-2024");
  EXPECT_EQ(year_month_day{leap_day}, 2024y / February / 29d) << "leap-day parsed incorrectly";
}

TEST(Date, MultiFormat_IncorrectInput) {
  using PP = pep::ParticipantPersonalia; // class under test
  using RequiredErrorT = std::exception;

  EXPECT_THROW(PP::ParseDateOfBirth("23-jun-20222"), RequiredErrorT) << "Should not accept excess Y-characters";
  EXPECT_THROW(PP::ParseDateOfBirth("23-sept-2022"), RequiredErrorT) << "Should not accept excess M-characters (abbrev)";
  EXPECT_THROW(PP::ParseDateOfBirth("23-008-2022"), RequiredErrorT) << "Should not accept excess M-characters (numeric)";
  EXPECT_THROW(PP::ParseDateOfBirth("010-jun-2022"), RequiredErrorT) << "Should not accept excess D-characters";

  EXPECT_THROW(PP::ParseDateOfBirth("00-01-2022"), RequiredErrorT) << "Should not accept zero as day";
  EXPECT_THROW(PP::ParseDateOfBirth("10-00-2022"), RequiredErrorT) << "Should not accept zero as month";

  EXPECT_THROW(PP::ParseDateOfBirth("32-jan-2025"), RequiredErrorT) << "Should not accept bogus day values";
  EXPECT_THROW(PP::ParseDateOfBirth("15-13-2025"), RequiredErrorT) << "Should not accept bogus month values";
  EXPECT_PRED1(DateNotOk, PP::ParseDateOfBirth("29-feb-2023")) << "Should only accept Feb 29th on leap-years";

  EXPECT_THROW(PP::ParseDateOfBirth("23052026"), RequiredErrorT) << "Should require separators";
  EXPECT_THROW(PP::ParseDateOfBirth("23/05/2026"), RequiredErrorT) << "Should not accept alternative separators";

  EXPECT_THROW(PP::ParseDateOfBirth(""), RequiredErrorT) << "Should not accept empty string";
}



}
