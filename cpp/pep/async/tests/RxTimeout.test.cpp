#include <gtest/gtest.h>

#include <pep/utils/Exceptions.hpp>
#include <pep/utils/TestTiming.hpp>
#include <pep/async/RxFinallyExhaust.hpp>
#include <pep/async/RxTimeout.hpp>

#include <boost/asio/io_context.hpp>
#include <optional>

using namespace std::literals;

/* This source demonstrates a bunch of problems running time-based observables on our ASIO RX scheduler.
 * Specifically, we were looking for a mechanism to issue errors for observables that don't produce
 * an .on_completed (or .on_error) before a specified deadline. Note that the standard RX .timeout operator
 * checks if the .on_next is invoked in time, but we initially believed that we could implement whole-observable
 * timeouts in terms of this anyway, e.g. using myObs.op(RxToVector()).timeout(...).
 */

namespace {

bool IsRxTimeoutError(std::exception_ptr exception) noexcept {
  if (exception == nullptr) {
    return false;
  }

  try {
    std::rethrow_exception(exception);
  }
  catch (const rxcpp::timeout_error&) {
    return true;
  }
  catch (...) {
    return false;
  }
}

/* E.g. if an observable wants to emit an item after LONG_TIME but times out after
 * SHORT_TIME, then the observable and the associated I/O context should terminate
 * (after the SHORT_TIME but) before the LONG_TIME.
 * Unfortunately, (machine load) circumstances may sometimes cause work to take more time than
 * (formally) required, which in turn may cause tests to fail. If so, increase the values of
 * SHORT_TIME and/or LONG_TIME (and/or the difference between them) to accommodate such
 * slow processing.
 */
const auto SHORT_TIME = 250ms;
const auto LONG_TIME = 500ms;

// Function typedefs/signatures that can be passed to the test function
using TimerObservable = rxcpp::observable<pep::FakeVoid>;
using MakeTimer = std::function<TimerObservable(pep::testing::Duration, boost::asio::io_context&)>;
using AddTimeout = std::function<TimerObservable(TimerObservable, pep::testing::Duration, boost::asio::io_context&)>;

const char* DescribeObservableState(const std::optional<std::exception_ptr>& error) {
  const char* result = "failed";
  if (!error.has_value()) {
    result = "running";
  }
  else if (*error == nullptr) {
    result = "completed successfully";
  }
  return result;
}

// Core test function: verifies proper functioning of the observable produced by "MakeTimer", and optionally of the observable produced by "AddTimeout"
void TestTimeBoundObservable(const MakeTimer& make_timer, const std::optional<AddTimeout>& add_timeout, bool should_time_out) {
  // Process parameters
  auto emit_after = SHORT_TIME;
  auto timeout_after = LONG_TIME;
  if (add_timeout.has_value()) {
    if (should_time_out) {
      std::swap(emit_after, timeout_after);
    }
  }
  else {
    ASSERT_FALSE(should_time_out) << "Pass an add_timeout function if you want to test timeouts";
  }

  // Helper variables
  boost::asio::io_context io_context;
  pep::testing::TimePoint start;

  // Create the timer observable
  start = pep::testing::Clock::now();
  TimerObservable observable = make_timer(emit_after, io_context);
  ASSERT_LT(pep::testing::MillisecondsSince(start), emit_after) << "Current thread was blocked by timer observable creation";

  // Optionally add the timeout
  if (add_timeout.has_value()) {
    start = pep::testing::Clock::now();
    observable = (*add_timeout)(observable, timeout_after, io_context);
    ASSERT_LT(pep::testing::MillisecondsSince(start), timeout_after) << "Current thread was blocked by addition of timeout to observable";
  }

  // Variables that'll receive the observable's output
  bool emitted = false;
  std::optional<std::exception_ptr> error; // Either an actual exception_ptr, or a nullptr if the observable completes successfully

  // Subscribe to the observable and assert stuff when the on_xyz notifications are invoked
  start = pep::testing::Clock::now();
  observable.subscribe(
    // on_next
    [&emitted, &error](pep::FakeVoid) {
      ASSERT_FALSE(emitted) << "Timer should produce a single value";
      EXPECT_FALSE(error.has_value()) << "Observable shouldn't produce a value when it's " << DescribeObservableState(error);
      emitted = true;
    },

    // on_error
    [&emitted, &error](std::exception_ptr exception) {
      ASSERT_FALSE(error.has_value()) << "Observable shouldn't produce an exception when it's " << DescribeObservableState(error);
      EXPECT_TRUE(IsRxTimeoutError(exception)) << "Observable should produce an RX timeout_error instead of a different exception: " << pep::GetExceptionMessage(exception);
      EXPECT_FALSE(emitted) << "Timeout shouldn't occur after timer has already produced a value";
      error = exception;
    },

    // on_completed
    [&emitted, &error]() {
      EXPECT_TRUE(emitted) << "Observable shouldn't complete successfully before having produced a value";
      ASSERT_FALSE(error.has_value()) << "Observable shouldn't complete successfully when it's " << DescribeObservableState(error);
      error = nullptr;
    }
  );
  ASSERT_LT(pep::testing::MillisecondsSince(start), SHORT_TIME) << "Subscribing to observable blocked current thread";
  ASSERT_FALSE(emitted) << "Observable shouldn't produce a value before having been scheduled";
  ASSERT_FALSE(error.has_value()) << "Observable shouldn't have " << DescribeObservableState(error) << " before having been scheduled";

  start = pep::testing::Clock::now();
  io_context.run();
  auto ran_for_msec = pep::testing::MillisecondsSince(start);

  EXPECT_TRUE(error.has_value()) << "Observable produced neither error nor completion notification";
  EXPECT_NE(should_time_out, emitted) << "Observable should either produce a value or time out";

  EXPECT_GE(ran_for_msec, SHORT_TIME) << "I/O context finished running before observable terminated";
  EXPECT_LT(ran_for_msec, LONG_TIME) << "I/O context kept running after observable terminated";
}

void TestTimer(const MakeTimer& make_timer) {
  TestTimeBoundObservable(make_timer, std::nullopt, false);
}

void TestTimeout(const MakeTimer& make_timer, const AddTimeout& add_timeout) {
  TestTimeBoundObservable(make_timer, add_timeout, true); // Timeout should occur before timer completes
  TestTimeBoundObservable(make_timer, add_timeout, false); // Timer should complete before timeout occurs
}


/* TEST FAILS:
// The call to .subscribe is blocking when we run a plain RX timer on the default (current thread) scheduler
TEST(RxTimeout, PlainTimer) {
  TestTimer(
    [](Duration emit_after, boost::asio::io_context&) {
      return rxcpp::observable<>::timer(emit_after);
    }
  );
}
// */

// Adding .subscribe_on to an rxcpp::observable<>::timer causes it to be scheduled on the Boost ASI/O context. But...
/* TEST SOMETIMES FAILS:
 * The I/O context sometimes finishes running before the "emit_after" duration has passed, so the "timer doesn't wait
 * long enough" in those cases. This may be caused by our ASIO scheduler using a deadline_timer, while this unit test
 * measures duration using a steady_clock.
 * TODO Is this still the case? It seems we're using a steady_timer now.
TEST(RxTimeout, TimerBoundToAsio) {
  TestTimer(
    [](Duration emit_after, boost::asio::io_context& io_context) {
      return rxcpp::observable<>::timer(emit_after)
        .subscribe_on(pep::observe_on_asio(io_context));
    }
  );
}
// */

/* TEST FAILS:
// Times out before completing: despite the observable already being subscribed on our ASIO scheduler, the call to .timeout is blocking.
// Completes before timeout:    the observable nevertheless produces an rxcpp::timeout_error, and the call to .subscribe is blocking.
TEST(RxTimeout, Plain) {
  TestTimeout(
    [](Duration emit_after, boost::asio::io_context& io_context) {
      return rxcpp::observable<>::timer(emit_after)
        .subscribe_on(pep::observe_on_asio(io_context));
    },
    [](TimerObservable items, Duration timeout_after, boost::asio::io_context& io_context) {
      return items
        .timeout(timeout_after);
    }
  );
}
// */

/* TEST FAILS:
// Completes before timeout:  the observable nevertheless produces an rxcpp::timeout_error, and the I/O context keeps running until that
//                            happens. Presumably the .timeout occurrence has been scheduled and isn't (or cannot be) unscheduled.
TEST(RxTimeout, BoundToAsio) {
  TestTimeout(
    [](Duration emit_after, boost::asio::io_context& io_context) {
      return rxcpp::observable<>::timer(emit_after)
        .subscribe_on(pep::observe_on_asio(io_context));
    },
    [](TimerObservable items, Duration timeout_after, boost::asio::io_context& io_context) {
      return items
        .timeout(timeout_after)
        .subscribe_on(pep::observe_on_asio(io_context)); // So we need a second .subscribe_on to schedule the .timeout on our I/O context
    }
  );
}
// */

// Since we can't get RX's native .timeout to work properly, we'll provide a replacement.
// We'll base it on a replacement for RX's native timer that is cancelled when the subscriber .unsubscribe-s from it.
TEST(RxTimeout, TimerReplacement) {
  TestTimer(
    [](pep::testing::Duration emit_after, boost::asio::io_context& io_context) {
      return pep::RxAsioTimer(duration_cast<pep::RxAsioDuration>(emit_after), io_context, pep::observe_on_asio(io_context));
    }
  );
}

// And here's our RxAsioTimeout replacement for RX's native .timeout.
// Since it passes the (unit) test, it has the properties that we require.
TEST(RxTimeout, TimeoutReplacement) {
  TestTimeout(
    [](pep::testing::Duration emit_after, boost::asio::io_context& io_context) {
      return pep::RxAsioTimer(duration_cast<pep::RxAsioDuration>(emit_after), io_context, pep::observe_on_asio(io_context));
    },
    [](TimerObservable items, pep::testing::Duration timeout_after, boost::asio::io_context& io_context) {
      return items.op(pep::RxAsioTimeout(duration_cast<pep::RxAsioDuration>(timeout_after), io_context, pep::observe_on_asio(io_context)));
    }
  );
}

// Verifies that an RxFinallyExhaust is applied even if an RxAsioTimeout occurs. See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2417
TEST(RxTimeout, FinallyExhaust) {
  boost::asio::io_context io_context;
  auto finished = pep::MakeSharedCopy(false);

  pep::RxAsioTimer(LONG_TIME, io_context, pep::observe_on_asio(io_context))
    .op(pep::RxAsioTimeout(SHORT_TIME, io_context, pep::observe_on_asio(io_context)))
    .op(pep::RxFinallyExhaust(io_context, [finished]() { *finished = true; return rxcpp::observable<>::empty<bool>(); }))
    .subscribe(
      [](pep::FakeVoid) { FAIL() << "Observable should time out instead of emitting a timer item"; },
      [](std::exception_ptr exception) { EXPECT_TRUE(IsRxTimeoutError(exception)) << "Observable should produce an RX timeout_error instead of a different exception: " << pep::GetExceptionMessage(exception); },
      []() { FAIL() << "Observable should time out instead of completing"; }
    );

  io_context.run();

  ASSERT_TRUE(*finished) << "Timeout shouldn't prevent finishing touch from be applied to observable";
}

}
