#include <gtest/gtest.h>

#include <pep/utils/ChronoUtil.hpp>

using namespace pep::chrono;
using namespace std::chrono_literals;

namespace {

TEST(ChronoUtil, ParseDuration) {
  ASSERT_EQ(ParseDuration<std::chrono::seconds>("10s"), 10s);
  ASSERT_EQ(ParseDuration<std::chrono::seconds>("10 s"), 10s);
  ASSERT_EQ(ParseDuration<std::chrono::seconds>("1 second"), 1s);
  ASSERT_EQ(ParseDuration<std::chrono::seconds>("10 seconds"), 10s);
  ASSERT_EQ(ParseDuration<std::chrono::minutes>("10min"), 10min);
  ASSERT_EQ(ParseDuration<std::chrono::minutes>("10 min"), 10min);
  ASSERT_EQ(ParseDuration<std::chrono::minutes>("1 minute"), 1min);
  ASSERT_EQ(ParseDuration<std::chrono::minutes>("10 minutes"), 10min);
  ASSERT_EQ(ParseDuration<std::chrono::hours>("10h"), 10h);
  ASSERT_EQ(ParseDuration<std::chrono::hours>("1 hour"), 1h);
  ASSERT_EQ(ParseDuration<std::chrono::hours>("10 hours"), 10h);
  ASSERT_EQ(ParseDuration<std::chrono::days>("1 day"), std::chrono::days(1));
  ASSERT_EQ(ParseDuration<std::chrono::days>("10 days"), std::chrono::days(10));
  ASSERT_EQ(ParseDuration<std::chrono::days>("10d"), std::chrono::days(10));
  ASSERT_EQ(ParseDuration<std::chrono::days>("10 d"), std::chrono::days(10));

  ASSERT_EQ(ParseDuration<std::chrono::seconds>("10min"), 600s);

  ASSERT_THROW(ParseDuration<std::chrono::seconds>("10 foo"), ParseException);
  ASSERT_THROW(ParseDuration<std::chrono::seconds>("s"), ParseException);
  ASSERT_THROW(ParseDuration<std::chrono::seconds>("10m"), ParseException);
  ASSERT_THROW(ParseDuration<std::chrono::seconds>("10ms"), ParseException);
  ASSERT_THROW(ParseDuration<std::chrono::seconds>("10 minutes days"), ParseException);
  ASSERT_THROW(ParseDuration<std::chrono::seconds>("s10s"), ParseException);
}

TEST(ChronoUtil, DurationToString) {
  ASSERT_EQ(ToString(std::chrono::seconds(0)), "0 seconds");
  ASSERT_EQ(ToString(std::chrono::seconds(1)), "1s");
  ASSERT_EQ(ToString(std::chrono::minutes(1)), "1m");
  ASSERT_EQ(ToString(std::chrono::hours(1)), "1h");
  ASSERT_EQ(ToString(std::chrono::days(1)), "1d");

  ASSERT_EQ(ToString(std::chrono::seconds(32000000)), "370d08h53m20s");
  ASSERT_EQ(ToString(std::chrono::seconds(60)), "1m");
  ASSERT_EQ(ToString(std::chrono::milliseconds(10)), "0.01s");
}

}
