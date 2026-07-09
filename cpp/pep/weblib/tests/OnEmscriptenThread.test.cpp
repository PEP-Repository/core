#include <pep/weblib/OnEmscriptenThread.hpp>

#include <pep/async/FakeVoid.hpp>

#include <emscripten/threading.h>

#include <gtest/gtest.h>

using namespace std::literals;

namespace {

TEST(OnEmscriptenThread, scheduleNow) {
  auto thread = rxcpp::rxs::just(pep::FakeVoid())
    // Because of -sPROXY_TO_PTHREAD, we are not the main thread.
    // We schedule this on the other (actual main) thread.
    .observe_on(pep::weblib::observe_on_emscripten_main_thread())
    .map([](pep::FakeVoid) {
      return ::pthread_self();
    })
    .as_blocking().last();
  EXPECT_EQ(thread, ::emscripten_main_runtime_thread_id());
}

TEST(OnEmscriptenThread, scheduleDelayed) {
  const auto when = std::chrono::steady_clock::now() + 200ms;
  auto [actualWhen, thread] = rxcpp::rxs::timer(when, pep::weblib::observe_on_emscripten_main_thread())
    .map([](long) {
      return std::pair{std::chrono::steady_clock::now(), ::pthread_self()};
    })
    .as_blocking().last();
  EXPECT_EQ(thread, ::emscripten_main_runtime_thread_id());
  EXPECT_GE(actualWhen, when);
}

}
