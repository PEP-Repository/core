#include <gtest/gtest.h>

#include <pep/networking/ExponentialBackoff.hpp>

namespace {

using namespace std::chrono_literals;

TEST(ExponentialBackoff, TimeoutAfter) {
  pep::ExponentialBackoff::Parameters parameters(1s, 10s, 3);
  EXPECT_EQ(parameters.timeoutAfter(0), 1s);
  EXPECT_EQ(parameters.timeoutAfter(1), 3s);
  EXPECT_EQ(parameters.timeoutAfter(2), 9s);
  EXPECT_EQ(parameters.timeoutAfter(3), 10s);
  EXPECT_EQ(parameters.timeoutAfter(1000), 10s);
}

}
