#include <pep/async/RxSubsequently.hpp>
#include <pep/utils/Defer.hpp>
#include <gtest/gtest.h>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-tap.hpp>

namespace {

struct Result {
  unsigned value_;
  unsigned recursion_;
};

using Inner = rxcpp::observable<Result>;

Inner MakeInner(std::shared_ptr<unsigned> counter, std::function<void()> andInvoke = []() {}) {
  assert(*counter != 0U);

  return pep::CreateObservable<Result>([counter, andInvoke](rxcpp::subscriber<Result> subscriber) {
    thread_local unsigned recursion = 0U;

    Result result{
      .value_ = (*counter)--,
      .recursion_ = recursion++,
    };
    PEP_DEFER(--recursion);

    subscriber.on_next(result);
    subscriber.on_completed();

    andInvoke();
    });
}

using ProduceNextInnerIfAvailable = std::function<void(std::shared_ptr<unsigned> counter, rxcpp::subscriber<Inner> subscriber)>;

bool RecursDuringCountdown(ProduceNextInnerIfAvailable produce) {
  bool result = false;

  pep::CreateObservable<Inner>([produce, counter = std::make_shared<unsigned>(5U)](rxcpp::subscriber<Inner> subscriber) {
    produce(counter, subscriber);
    })
    .concat_map([](Inner inner) { return inner; })
    .subscribe([&result](Result entry) {
        if (entry.recursion_ != 0U) {
          result = true;
        }
      });

  return result;
}

void ProduceNextUsingBunnyHopping(std::shared_ptr<unsigned> counter, rxcpp::subscriber<Inner> subscriber) {
  if (*counter != 0U) {
    subscriber.on_next(MakeInner(counter, [counter, subscriber]() {ProduceNextUsingBunnyHopping(counter, subscriber); }));
  }
  else {
    subscriber.on_completed();
  }
}

void ProduceNextUsingRxNativeTap(std::shared_ptr<unsigned> counter, rxcpp::subscriber<Inner> subscriber) {
  if (*counter != 0U) {
    subscriber.on_next(MakeInner(counter)
      .tap(
        [](auto) { /* ignore */},
        [](std::exception_ptr) { /* ignore (or forward to outer?) */},
        [counter, subscriber]() {ProduceNextUsingRxNativeTap(counter, subscriber); }
      ));
  } else {
    subscriber.on_completed();
  }
}

void ProduceNextUsingRxSubsequently(std::shared_ptr<unsigned> counter, rxcpp::subscriber<Inner> subscriber) {
  if (*counter != 0U) {
    subscriber.on_next(MakeInner(counter)
      .op(pep::RxSubsequently([counter, subscriber]() {ProduceNextUsingRxSubsequently(counter, subscriber); })));
  } else {
    subscriber.on_completed();
  }
}

}

TEST(RxSubsequently, PreventsRecursion) {
  // We expect that recursion (for every entry in the outer observable) will cause trouble such as stack overflows...
  EXPECT_TRUE(RecursDuringCountdown(&ProduceNextUsingBunnyHopping))     << "Bunny hopping causes recursive calls";
  EXPECT_TRUE(RecursDuringCountdown(&ProduceNextUsingRxNativeTap))      << "Tapping causes recursive calls";
  // ...which prompted us to create the non-recursive RxSubsequently
  EXPECT_FALSE(RecursDuringCountdown(&ProduceNextUsingRxSubsequently))  << "RxSubsequently shouldn't cause recursive calls";
}
