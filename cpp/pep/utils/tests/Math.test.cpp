#include <gtest/gtest.h>
#include <pep/utils/Math.hpp>

namespace {

TEST(Math, CeilDiv) {
  EXPECT_EQ(pep::CeilDiv(40u, 1u), 40);
  EXPECT_EQ(pep::CeilDiv(40u, 10u), 4);
  EXPECT_EQ(pep::CeilDiv(0u, 1u), 0);
  EXPECT_EQ(pep::CeilDiv(41u, 10u), 5);
  EXPECT_EQ(pep::CeilDiv(42u, 10u), 5);
  EXPECT_EQ(pep::CeilDiv(49u, 10u), 5);
}

TEST(Math, IntegralLog) {
  EXPECT_EQ(pep::IntegralLog(0b000000001U, 2U), 0U);
  EXPECT_EQ(pep::IntegralLog(0b000000010U, 2U), 1U);
  EXPECT_EQ(pep::IntegralLog(0b000000100U, 2U), 2U);
  EXPECT_EQ(pep::IntegralLog(0b000001000U, 2U), 3U);
  EXPECT_EQ(pep::IntegralLog(0b000010000U, 2U), 4U);
  EXPECT_EQ(pep::IntegralLog(0b000100000U, 2U), 5U);
  EXPECT_EQ(pep::IntegralLog(0b001000000U, 2U), 6U);
  EXPECT_EQ(pep::IntegralLog(0b010000000U, 2U), 7U);
  EXPECT_EQ(pep::IntegralLog(0b100000000U, 2U), 8U);

  EXPECT_EQ(pep::IntegralLog(0b000000101U, 2U), std::nullopt);
  EXPECT_EQ(pep::IntegralLog(0b000001010U, 2U), std::nullopt);
  EXPECT_EQ(pep::IntegralLog(0b000010100U, 2U), std::nullopt);
  EXPECT_EQ(pep::IntegralLog(0b000101000U, 2U), std::nullopt);
  EXPECT_EQ(pep::IntegralLog(0b001010000U, 2U), std::nullopt);
  EXPECT_EQ(pep::IntegralLog(0b010100000U, 2U), std::nullopt);
  EXPECT_EQ(pep::IntegralLog(0b101000000U, 2U), std::nullopt);

  for (uint64_t i = 0U; i < 64U; ++i) {
    auto value = uint64_t{1} << i;
    EXPECT_EQ(pep::IntegralLog(value, uint64_t{2}), i) << "Base-2 logarithm not found for bit " << i;
  }

  EXPECT_EQ(pep::IntegralLog(1U, 10U), 0U);
  EXPECT_EQ(pep::IntegralLog(10U, 10U), 1U);
  EXPECT_EQ(pep::IntegralLog(100U, 10U), 2U);
  EXPECT_EQ(pep::IntegralLog(1000U, 10U), 3U);

  EXPECT_EQ(pep::IntegralLog(3U, 10U), std::nullopt);
  EXPECT_EQ(pep::IntegralLog(42U, 10U), std::nullopt);
  EXPECT_EQ(pep::IntegralLog(999U, 10U), std::nullopt);
  EXPECT_EQ(pep::IntegralLog(4321U, 10U), std::nullopt);
}

}
