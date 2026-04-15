#include <pep/async/RxSubsequently.hpp>
#include <pep/utils/Defer.hpp>
#include <gtest/gtest.h>
#include <rxcpp/operators/rx-concat.hpp>
#include <rxcpp/operators/rx-tap.hpp>
#include <numeric>

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

void TestIterativeCountDown(ProduceNextInnerIfAvailable produce, bool recurs, const char* description) {
  constexpr unsigned count = 5U;
  std::vector<unsigned> produced;
  bool recursion_detected = false;

  pep::CreateObservable<Inner>([produce, counter = std::make_shared<unsigned>(count)](rxcpp::subscriber<Inner> subscriber) {
    produce(counter, subscriber);
    })
    .concat()
    .subscribe([&recursion_detected, &produced](Result entry) {
        produced.emplace_back(entry.value_);
        if (entry.recursion_ != 0U) {
          recursion_detected = true;
        }
      });

  // Validate our primary concern
  EXPECT_EQ(recurs, recursion_detected) << description << (recurs ? "should" : "shouldn't") << " cause recursive calls";

  // Validate the "produce" function's results
  std::vector<unsigned> expected(count);
  std::iota(expected.begin(), expected.end(), 1U);
  std::reverse(expected.begin(), expected.end());
  EXPECT_EQ(produced, expected);
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
  TestIterativeCountDown(&ProduceNextUsingBunnyHopping, true, "Bunny hopping");
  TestIterativeCountDown(&ProduceNextUsingRxNativeTap,  true, "Tapping");
  // ...which prompted us to create the non-recursive RxSubsequently
  TestIterativeCountDown(&ProduceNextUsingRxSubsequently, false, "RxSubsequently");
}
