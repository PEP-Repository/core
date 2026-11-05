#include <pep/utils/Action.hpp>
#include <pep/utils/Base64.hpp>
#include <pep/utils/Random.hpp>

#include <gmock/gmock-matchers.h>
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
  // Make a random(ized) "what" text
  auto encoded = pep::EncodeBase64Url(pep::RandomString(64U));

  EXPECT_THAT( // Ensure that...
    [encoded]() { pep::PerformAsAction(&JustInvokeItAlready, [&encoded]() {throw std::runtime_error(encoded); }); }, // ... when the callback raises an exception...
    ::testing::ThrowsMessage<std::runtime_error>(::testing::StrEq(encoded))); // ... the "PerformAsAction" function propagates that original exception
}
