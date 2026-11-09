#include <pep/utils/Action.hpp>
#include <pep/utils/TestError.hpp>
#include <gtest/gtest.h>

namespace {

void JustInvokeItAlready(const pep::Action& action) {
  action();
}

}

TEST(Action, SupportsHeterogeneousSignatures) {
  EXPECT_EQ(42, pep::PerformAsAction(&JustInvokeItAlready, []() { return 42; }));
  EXPECT_EQ(48, pep::PerformAsAction(&JustInvokeItAlready, [](int lhs, int rhs) { return lhs + rhs; }, 46, 2));

  int instance = 365;
  int& returned = pep::PerformAsAction(&JustInvokeItAlready, [&]() -> int& { return instance; });
  EXPECT_EQ(std::addressof(instance), std::addressof(returned));
}

TEST(Action, PropagatesExceptions) {
  using pep::TestError;

  EXPECT_THROW(pep::PerformAsAction(&JustInvokeItAlready, []() {throw TestError(); }), // When the callback raises an exception...
    TestError); // ... the "PerformAsAction" function should propagate that original exception
}
