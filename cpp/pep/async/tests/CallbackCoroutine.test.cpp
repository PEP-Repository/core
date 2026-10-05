#include <pep/async/CallbackCoroutine.hpp>

#include <pep/utils/Defer.hpp>
#include <pep/utils/TestError.hpp>

#include <gtest/gtest.h>

using namespace pep;

namespace {

CallbackCoroutine<int> success(std::function<void(int)>, std::function<void()>) {
  co_return 42;
}
TEST(CallbackCoroutine, success) {
  bool callbackRan = false;
  success([&](int i) {
    EXPECT_EQ(i, 42);
    callbackRan = true;
  }, [] {
    EXPECT_NO_THROW(throw);
  });
  EXPECT_TRUE(callbackRan);
}

CallbackCoroutine<void> successVoid(std::function<void()>, std::function<void()>) {
  co_return;
}
TEST(CallbackCoroutine, successVoid) {
  bool callbackRan = false;
  successVoid([&] {
    callbackRan = true;
  }, [] {
    EXPECT_NO_THROW(throw);
  });
  EXPECT_TRUE(callbackRan);
}

CallbackCoroutine<int> params(std::function<void(int)>, std::function<void()>, int i) {
  co_return i;
}
TEST(CallbackCoroutine, params) {
  bool callbackRan = false;
  params([&](int i) {
    EXPECT_EQ(i, 42);
    callbackRan = true;
  }, [] {
    EXPECT_NO_THROW(throw);
  }, 42);
  EXPECT_TRUE(callbackRan);
}

CallbackCoroutine<int> fail(std::function<void(int)>, std::function<void()>, bool doThrow = true) {
  if (doThrow) { throw TestError{}; }
  co_return 42;
}
TEST(CallbackCoroutine, fail) {
  bool callbackRan = false;
  fail([](int i) {
    FAIL() << "Should throw, but got " << i;
  }, [&] {
    EXPECT_THROW(throw, TestError);
    callbackRan = true;
  });
  EXPECT_TRUE(callbackRan);
}

TEST(CallbackCoroutine, errorInCallback) {
  bool callbackRan = false;
  success([](int) {
    throw TestError{};
  }, [&] {
    EXPECT_THROW(throw, TestError);
    callbackRan = true;
  });
  EXPECT_TRUE(callbackRan) << "Error in callback should be passed to error callback";
}

CallbackCoroutine<int> observeLocalDestruction(std::function<void(int)>, std::function<void()>, bool* destructed) {
  PEP_DEFER(*destructed = true);
  co_return 42;
}
TEST(CallbackCoroutine, localDestruction) {
  bool localDestructed = false;
  bool callbackRan = false;
  observeLocalDestruction([&](int) {
    EXPECT_TRUE(localDestructed) << "Local variable should be destructed before callback";
    callbackRan = true;
  }, [] {
    EXPECT_NO_THROW(throw);
  }, &localDestructed);
  EXPECT_TRUE(callbackRan);
}

}
