#include <pep/networking/MessageProperties.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>

using namespace pep;

namespace {

TEST(MessageFlags, constructor_rejects_invalid_flags_with_details) {
  ASSERT_THROW(MessageFlags(false, true, true), std::invalid_argument);
  try {
    (void) MessageFlags(false, true, true);
    FAIL();
  } catch (const std::invalid_argument& ex) {
    EXPECT_PRED1([](std::string_view str) {return str.ends_with(": error, payload");}, ex.what());
  }
}

}
