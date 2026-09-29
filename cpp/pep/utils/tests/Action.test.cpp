#include <pep/utils/Action.hpp>
#include <pep/utils/Base64.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/TestMacros.hpp>

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
  constexpr size_t bytes = 64U;
  char raw[bytes];
  pep::RandomBytes(raw);
  auto encoded = pep::EncodeBase64Url(std::string(raw, bytes));

  PEP_EXPECT_THROWS_MESSAGE( // Ensure that...
    pep::PerformAsAction(&JustInvokeItAlready, [&encoded]() {throw std::runtime_error(encoded); }), // ... when the callback raises an exception...
    std::runtime_error,
    ::testing::StrCaseEq(encoded)); // ... the "PerformAsAction" function fails with that original exception
}
