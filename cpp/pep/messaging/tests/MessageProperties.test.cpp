#include <pep/messaging/MessageProperties.hpp>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

namespace pep::messaging {
namespace {

std::string ToString(Flags flags) { return (std::ostringstream{} << flags).str(); }

using ::testing::HasSubstr;
using ::testing::Property;
using ::testing::Throws;

TEST(AssertValidCombination_Flags, rejects_error_without_close) {
  EXPECT_THAT(
      [] { AssertValidCombination(Flags::Error); },
      Throws<std::invalid_argument>(
          Property(&std::invalid_argument::what, HasSubstr(ToString(Flags::Error)))));
}

TEST(AssertValidCombination_Flags, rejects_error_with_payload) {
  EXPECT_THAT(
      [] { AssertValidCombination(Flags::Error | Flags::Payload); },
      Throws<std::invalid_argument>(
          Property(&std::invalid_argument::what, HasSubstr(ToString(Flags::Error | Flags::Payload)))));

  EXPECT_THAT(
      [] { AssertValidCombination(Flags::Error | Flags::Payload | Flags::Close); },
      Throws<std::invalid_argument>(
          Property(&std::invalid_argument::what, HasSubstr(ToString(Flags::Error | Flags::Payload | Flags::Close)))));
}

TEST(AssertValidCombination_Flags, accepts_valid_combinations) {
  ASSERT_NO_THROW(AssertValidCombination(Flags::Close));
  ASSERT_NO_THROW(AssertValidCombination(Flags::Payload));
  ASSERT_NO_THROW(AssertValidCombination(Flags::Payload | Flags::Close));
  ASSERT_NO_THROW(AssertValidCombination(Flags::Error | Flags::Close));
}

}
}
