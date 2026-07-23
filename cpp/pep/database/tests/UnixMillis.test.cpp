#include <pep/database/UnixMillis.hpp>
#include <gtest/gtest.h>

using namespace pep::database;

TEST(UnixMillis, RoundTripToTimestamp) {
  const auto TsEpoch = pep::Timestamp{}, TsNow = pep::TimeNow();
  const auto UmEpoch = UnixMillis(0);

  EXPECT_EQ(UmEpoch, TimestampToUnixMillis(TsEpoch));
  EXPECT_EQ(TsEpoch, TimestampFromUnixMillis(UmEpoch));

  auto now = TimestampToUnixMillis(TsNow);
  EXPECT_NE(UmEpoch, now);
  EXPECT_EQ(TsNow, TimestampFromUnixMillis(now));
}
