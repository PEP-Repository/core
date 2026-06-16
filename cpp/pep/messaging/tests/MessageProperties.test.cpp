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

TEST(AssertValidCombination_Flags, rejects_error_without_close) {
  EXPECT_THAT(
      [] { Flags{Flags::Bits::Error}; },
      Throws<std::invalid_argument>(
          Property(&std::invalid_argument::what, HasSubstr(ToString(Flags::Bits::Error)))));
}

TEST(AssertValidCombination_Flags, rejects_error_with_payload) {
  EXPECT_THAT(
      [] { Flags{Flags::Bits::Error | Flags::Bits::Payload}; },
      Throws<std::invalid_argument>(
          Property(&std::invalid_argument::what, HasSubstr(ToString(Flags::Bits::Error | Flags::Bits::Payload)))));

  EXPECT_THAT(
      [] { Flags{Flags::Bits::Error | Flags::Bits::Payload | Flags::Bits::Close}; },
      Throws<std::invalid_argument>(
          Property(&std::invalid_argument::what, HasSubstr(ToString(Flags::Bits::Error | Flags::Bits::Payload | Flags::Bits::Close)))));
}

TEST(AssertValidCombination_Flags, accepts_valid_combinations) {
  ASSERT_NO_THROW(Flags{Flags::Bits::Close});
  ASSERT_NO_THROW(Flags{Flags::Bits::Payload});
  ASSERT_NO_THROW(Flags{Flags::Bits::Payload | Flags::Bits::Close});
  ASSERT_NO_THROW(Flags{Flags::Bits::Error | Flags::Bits::Close});
}

}
}
