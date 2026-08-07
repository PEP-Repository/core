#include <pep/messaging/MessageProperties.hpp>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

namespace pep::messaging {
namespace {

std::string ToString(Flags::Bits flags) { return (std::ostringstream{} << flags).str(); }

using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::Throws;

TEST(MessagingFlags, constructor_rejects_error_without_close) {
  EXPECT_THAT(
      [] { Flags::TestPrivateConstructor(Flags::Bits::Error); },
      Throws<std::invalid_argument>(
          Property(&std::invalid_argument::what, HasSubstr(ToString(Flags::Bits::Error)))));
}

TEST(MessagingFlags, constructor_rejects_error_with_payload) {
  EXPECT_THAT(
      [] { Flags::TestPrivateConstructor(Flags::Bits::Error | Flags::Bits::Payload); },
      Throws<std::invalid_argument>(
          Property(&std::invalid_argument::what, HasSubstr(ToString(Flags::Bits::Error | Flags::Bits::Payload)))));

  EXPECT_THAT(
      [] { Flags::TestPrivateConstructor(Flags::Bits::Error | Flags::Bits::Payload | Flags::Bits::Close); },
      Throws<std::invalid_argument>(
          Property(&std::invalid_argument::what, HasSubstr(ToString(Flags::Bits::Error | Flags::Bits::Payload | Flags::Bits::Close)))));
}

TEST(MessagingFlags, has) {
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

TEST(MessagingFlags, withClose) {
  EXPECT_EQ(Flags::None.withClose(), Flags::Close);
  EXPECT_EQ(Flags::Close.withClose(), Flags::Close);
  EXPECT_EQ(Flags::Error.withClose(), Flags::Error);
  EXPECT_EQ(Flags::Payload.withClose(), Flags::ClosingPayload);
  EXPECT_EQ(Flags::ClosingPayload.withClose(), Flags::ClosingPayload);
}

} // namespace
} // namespace pep::messaging
