#include <pep/utils/Action.hpp>
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
  static_assert(!std::is_base_of_v<std::runtime_error, std::bad_optional_access>, "Can't discriminate our sentinel exception from possible exception thrown by PerformAsAction function");

  EXPECT_THROW(pep::PerformAsAction(&JustInvokeItAlready, []() {throw std::runtime_error("Should propagate"); }), std::runtime_error);
}
