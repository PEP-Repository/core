#include <pep/async/ObservableAwaiter.hpp>

#include <pep/async/CallbackCoroutine.hpp>
#include <pep/utils/TestError.hpp>

#include <rxcpp/operators/rx-concat.hpp>

#include <gtest/gtest.h>

using namespace pep;

namespace {

CallbackCoroutine<int> awaitSingle(std::function<void(int)>, std::function<void()>) {
  co_return co_await rxcpp::observable<>::just(42);
}
TEST(ObservableAwaiter, single) {
  bool callbackRan = false;
  awaitSingle([&](int i) {
    EXPECT_EQ(i, 42);
    callbackRan = true;
  }, [] {
    EXPECT_NO_THROW(throw);
  });
  EXPECT_TRUE(callbackRan);
}

CallbackCoroutine<int> awaitMulti(std::function<void(int)>, std::function<void()>) {
  co_return co_await rxcpp::observable<>::range(0, 1);
}
TEST(ObservableAwaiter, multi) {
  bool callbackRan = false;
  //NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Assign) False positive in ObservableAwaiter with old Clang-Tidy 18
  awaitMulti([](int i) {
    FAIL() << "Should throw on multiple values, but got " << i;
  }, [&] {
    EXPECT_THROW(throw, std::runtime_error);
    callbackRan = true;
  });
  EXPECT_TRUE(callbackRan);
}

CallbackCoroutine<int> awaitNone(std::function<void(int)>, std::function<void()>) {
  co_return co_await rxcpp::observable<>::empty<int>();
}
TEST(ObservableAwaiter, none) {
  bool callbackRan = false;
  awaitNone([](int i) {
    FAIL() << "Should throw on empty observable, but got " << i;
  }, [&] {
    EXPECT_THROW(throw, std::runtime_error);
    callbackRan = true;
  });
  EXPECT_TRUE(callbackRan);
}

CallbackCoroutine<int> awaitError(std::function<void(int)>, std::function<void()>) {
  // Prepend element to check that only the error gets propagated
  co_return co_await rxcpp::observable<>::just(42).concat(rxcpp::observable<>::error<int>(TestError{}));
}
TEST(ObservableAwaiter, error) {
  bool callbackRan = false;
  awaitError([](int i) {
    FAIL() << "Should throw on error, but got " << i;
  }, [&] {
    EXPECT_THROW(throw, TestError);
    callbackRan = true;
  });
  EXPECT_TRUE(callbackRan);
}

}
