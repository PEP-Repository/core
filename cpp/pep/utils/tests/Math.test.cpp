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

}
