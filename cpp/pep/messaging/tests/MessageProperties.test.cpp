#include <pep/messaging/MessageProperties.hpp>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

namespace pep::messaging {
namespace {

TEST(MessagingFlags, Relations) {
  EXPECT_TRUE(Flags::None.has(Flags::None));
  EXPECT_FALSE(Flags::None.has(Flags::Close));
  EXPECT_FALSE(Flags::None.has(Flags::Error));
  EXPECT_FALSE(Flags::None.has(Flags::Payload));
  EXPECT_FALSE(Flags::None.has(Flags::ClosingPayload));

  EXPECT_TRUE(Flags::Close.has(Flags::None));
  EXPECT_TRUE(Flags::Close.has(Flags::Close));
  EXPECT_FALSE(Flags::Close.has(Flags::Error));
  EXPECT_FALSE(Flags::Close.has(Flags::Payload));
  EXPECT_FALSE(Flags::Close.has(Flags::ClosingPayload));

  EXPECT_TRUE(Flags::Error.has(Flags::None));
  EXPECT_TRUE(Flags::Error.has(Flags::Close));
  EXPECT_TRUE(Flags::Error.has(Flags::Error));
  EXPECT_FALSE(Flags::Error.has(Flags::Payload));
  EXPECT_FALSE(Flags::Error.has(Flags::ClosingPayload));

  EXPECT_TRUE(Flags::Payload.has(Flags::None));
  EXPECT_FALSE(Flags::Payload.has(Flags::Close));
  EXPECT_FALSE(Flags::Payload.has(Flags::Error));
  EXPECT_TRUE(Flags::Payload.has(Flags::Payload));
  EXPECT_FALSE(Flags::Payload.has(Flags::ClosingPayload));

  EXPECT_TRUE(Flags::ClosingPayload.has(Flags::None));
  EXPECT_TRUE(Flags::ClosingPayload.has(Flags::Close));
  EXPECT_TRUE(Flags::ClosingPayload.has(Flags::Payload));
  EXPECT_FALSE(Flags::ClosingPayload.has(Flags::Error));
  EXPECT_TRUE(Flags::ClosingPayload.has(Flags::ClosingPayload));
}

TEST(MessagingFlags, Transformations) {
  EXPECT_EQ(Flags::None.withClose(), Flags::Close);
  EXPECT_EQ(Flags::Close.withClose(), Flags::Close);
  EXPECT_EQ(Flags::Error.withClose(), Flags::Error);
  EXPECT_EQ(Flags::Payload.withClose(), Flags::ClosingPayload);
  EXPECT_EQ(Flags::ClosingPayload.withClose(), Flags::ClosingPayload);
}

} // namespace
} // namespace pep::messaging
