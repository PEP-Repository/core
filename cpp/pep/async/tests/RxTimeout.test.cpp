#include <gtest/gtest.h>

#include <pep/utils/Exceptions.hpp>
#include <pep/async/RxFinallyExhaust.hpp>
#include <pep/async/RxTimeout.hpp>

#include <optional>

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

// (Properties of) the clock type used for timing-based tests
typedef std::chrono::steady_clock Clock;
typedef Clock::time_point TimePoint;
typedef Clock::duration Duration;

/* To prevent gtest from reporting std::chrono::duration values as e.g. "8-byte-object <48-F1 AC-0F 00-00 00-00>",
 * we specify values in milliseconds, which we express as a fundamental signed integral type
 * (e.g. long long in my implementation).
 */
typedef std::chrono::milliseconds::rep Milliseconds;

constexpr Duration MillisecondsToDuration(Milliseconds msec) {
  return std::chrono::duration_cast<Duration>(std::chrono::milliseconds(msec));
}

/* E.g. if an observable wants to emit an item after LONG_MSEC but times out after
 * SHORT_MSEC, then the observable and the associated I/O service should terminate
 * (after the SHORT_MSEC but) before the LONG_MSEC.
 * Unfortunately, (machine load) circumstances may sometimes cause work to take more time than
 * (formally) required, which in turn may cause tests to fail. If so, increase the values of
 * SHORT_MSEC and/or LONG_MSEC (and/or the difference between them) to accommodate such
 * slow processing.
 */
const auto SHORT_MSEC = 250;
const auto LONG_MSEC = 500;

// Function typedefs/signatures that can be passed to the test function
typedef rxcpp::observable<pep::FakeVoid> TimerObservable;
typedef std::function<
  TimerObservable(Duration, boost::asio::io_service&)
  > MakeTimer;
typedef std::function<
  TimerObservable(TimerObservable, Duration, boost::asio::io_service&)
  > AddTimeout;

// Utility functions
Milliseconds GetMillisecondsSince(const TimePoint& time_point) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - time_point).count();
}

const char* DescribeObservableState(const std::optional<std::exception_ptr>& error) {
  const char* result;
  if (!error.has_value()) {
    result = "running";
  }
  else if (*error == nullptr) {
    result = "completed successfully";
  }
  else {
    result = "failed";
  }
  return result;
}

// Core test function: verifies proper functioning of the observable produced by "MakeTimer", and optionally of the observable produced by "AddTimeout"
void TestTimeBoundObservable(const MakeTimer& make_timer, const std::optional<AddTimeout>& add_timeout, bool should_time_out) {
  // Process parameters
  Milliseconds emit_after = SHORT_MSEC, timeout_after = LONG_MSEC;
  if (add_timeout.has_value()) {
    if (should_time_out) {
      std::swap(emit_after, timeout_after);
    }
  }
  else {
    ASSERT_FALSE(should_time_out) << "Pass an add_timeout function if you want to test timeouts";
  }

  // Helper variables
  boost::asio::io_service io_service;
  TimePoint start;

  // Create the timer observable
  start = Clock::now();
  TimerObservable observable = make_timer(MillisecondsToDuration(emit_after), io_service);
  ASSERT_LT(GetMillisecondsSince(start), emit_after) << "Current thread was blocked by timer observable creation";

  // Optionally add the timeout
  if (add_timeout.has_value()) {
    start = Clock::now();
    observable = (*add_timeout)(observable, MillisecondsToDuration(timeout_after), io_service);
    ASSERT_LT(GetMillisecondsSince(start), timeout_after) << "Current thread was blocked by addition of timeout to observable";
  }

  // Variables that'll receive the observable's output
  bool emitted = false;
  std::optional<std::exception_ptr> error; // Either an actual exception_ptr, or a nullptr if the observable completes successfully

  // Subscribe to the observable and assert stuff when the on_xyz notifications are invoked
  start = Clock::now();
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
  ASSERT_LT(GetMillisecondsSince(start), SHORT_MSEC) << "Subscribing to observable blocked current thread";
  ASSERT_FALSE(emitted) << "Observable shouldn't produce a value before having been scheduled";
  ASSERT_FALSE(error.has_value()) << "Observable shouldn't have " << DescribeObservableState(error) << " before having been scheduled";

  start = Clock::now();
  io_service.run();
  auto ran_for_msec = GetMillisecondsSince(start);

  EXPECT_TRUE(error.has_value()) << "Observable produced neither error nor completion notification";
  EXPECT_NE(should_time_out, emitted) << "Observable should either produce a value or time out";

  EXPECT_GE(ran_for_msec, SHORT_MSEC) << "I/O service finished running before observable terminated";
  EXPECT_LT(ran_for_msec, LONG_MSEC) << "I/O service kept running after observable terminated";
}

void TestTimer(const MakeTimer& make_timer) {
  TestTimeBoundObservable(make_timer, std::nullopt, false);
}

void TestTimeout(const MakeTimer& make_timer, const AddTimeout& add_timeout) {
  TestTimeBoundObservable(make_timer, add_timeout, true); // Timeout should occur before timer completes
  TestTimeBoundObservable(make_timer, add_timeout, false); // Timer should complete before timeout occurs
}

}

/* TEST FAILS:
// The call to .subscribe is blocking when we run a plain RX timer on the default (current thread) scheduler
TEST(RxTimeout, PlainTimer) {
  TestTimer(
    [](Duration emit_after, boost::asio::io_service&) {
      return rxcpp::observable<>::timer(emit_after);
    }
  );
}
// */

// Adding .subscribe_on to an rxcpp::observable<>::timer causes it to be scheduled on the Boost ASIO service. But...
/* TEST SOMETIMES FAILS:
 * The I/O service sometimes finishes running before the "emit_after" duration has passed, so the "timer doesn't wait
 * long enough" in those cases. This may be caused by our ASIO scheduler using a deadline_timer, while this unit test
 * measures duration using a steady_clock.
TEST(RxTimeout, TimerBoundToAsio) {
  TestTimer(
    [](Duration emit_after, boost::asio::io_service& io_service) {
      return rxcpp::observable<>::timer(emit_after)
        .subscribe_on(pep::observe_on_asio(io_service));
    }
  );
}
// */

/* TEST FAILS:
// Times out before completing: despite the observable already being subscribed on our ASIO scheduler, the call to .timeout is blocking.
// Completes before timeout:    the observable nevertheless produces an rxcpp::timeout_error, and the call to .subscribe is blocking.
TEST(RxTimeout, Plain) {
  TestTimeout(
    [](Duration emit_after, boost::asio::io_service& io_service) {
      return rxcpp::observable<>::timer(emit_after)
        .subscribe_on(pep::observe_on_asio(io_service));
    },
    [](TimerObservable items, Duration timeout_after, boost::asio::io_service& io_service) {
      return items
        .timeout(timeout_after);
    }
  );
}
// */

/* TEST FAILS:
// Completes before timeout:  the observable nevertheless produces an rxcpp::timeout_error, and the I/O service keeps running until that
//                            happens. Presumably the .timeout occurrence has been scheduled and isn't (or cannot be) unscheduled.
TEST(RxTimeout, BoundToAsio) {
  TestTimeout(
    [](Duration emit_after, boost::asio::io_service& io_service) {
      return rxcpp::observable<>::timer(emit_after)
        .subscribe_on(pep::observe_on_asio(io_service));
    },
    [](TimerObservable items, Duration timeout_after, boost::asio::io_service& io_service) {
      return items
        .timeout(timeout_after)
        .subscribe_on(pep::observe_on_asio(io_service)); // So we need a second .subscribe_on to schedule the .timeout on our I/O service
    }
  );
}
// */

// Since we can't get RX's native .timeout to work properly, we'll provide a replacement.
// We'll base it on a replacement for RX's native timer that is cancelled when the subscriber .unsubscribe-s from it.
TEST(RxTimeout, TimerReplacement) {
  TestTimer(
    [](Duration emit_after, boost::asio::io_service& io_service) {
      return pep::RxAsioTimer(std::chrono::duration_cast<pep::RxAsioDuration>(emit_after), io_service, pep::observe_on_asio(io_service));
    }
  );
}

// And here's our RxAsioTimeout replacement for RX's native .timeout.
// Since it passes the (unit) test, it has the properties that we require.
TEST(RxTimeout, TimeoutReplacement) {
  TestTimeout(
    [](Duration emit_after, boost::asio::io_service& io_service) {
      return pep::RxAsioTimer(std::chrono::duration_cast<pep::RxAsioDuration>(emit_after), io_service, pep::observe_on_asio(io_service));
    },
    [](TimerObservable items, Duration timeout_after, boost::asio::io_service& io_service) {
      return items.op(pep::RxAsioTimeout(std::chrono::duration_cast<pep::RxAsioDuration>(timeout_after), io_service, pep::observe_on_asio(io_service)));
    }
  );
}

// Verifies that an RxFinallyExhaust is applied even if an RxAsioTimeout occurs. See https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2417
TEST(RxTimeout, FinallyExhaust) {
  boost::asio::io_service io_service;
  auto finished = pep::MakeSharedCopy(false);

  pep::RxAsioTimer(std::chrono::duration_cast<pep::RxAsioDuration>(std::chrono::milliseconds(LONG_MSEC)), io_service, pep::observe_on_asio(io_service))
    .op(pep::RxAsioTimeout(std::chrono::duration_cast<pep::RxAsioDuration>(std::chrono::milliseconds(SHORT_MSEC)), io_service, pep::observe_on_asio(io_service)))
    .op(pep::RxFinallyExhaust(io_service, [finished]() { *finished = true; return rxcpp::observable<>::empty<bool>(); }))
    .subscribe(
      [](pep::FakeVoid) { FAIL() << "Observable should time out instead of emitting a timer item"; },
      [](std::exception_ptr exception) { EXPECT_TRUE(IsRxTimeoutError(exception)) << "Observable should produce an RX timeout_error instead of a different exception: " << pep::GetExceptionMessage(exception); },
      []() { FAIL() << "Observable should time out instead of completing"; }
    );

  io_service.run();

  ASSERT_TRUE(*finished) << "Timeout shouldn't prevent finishing touch from be applied to observable";
}
