#include <pep/async/RxSubsequently.hpp>
#include <pep/utils/Defer.hpp>
#include <gtest/gtest.h>
#include <rxcpp/operators/rx-concat_map.hpp>

namespace {

std::shared_ptr<unsigned> CreateCounter() {
  return std::make_shared<unsigned>(5U);
}

struct Result {
  unsigned value_;
  unsigned recursion_;
};

using Inner = rxcpp::observable<Result>;
using Outer = rxcpp::observable<Inner>;

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

using ProduceNextInner = std::function<void(std::shared_ptr<unsigned> counter, rxcpp::subscriber<Inner> subscriber)>;

unsigned MaxRecursionsDuringCountDown(ProduceNextInner produceNextInner) {
  unsigned result = 0U;

  pep::CreateObservable<Inner>([produceNextInner, counter = CreateCounter()](rxcpp::subscriber<Inner> subscriber) {
    produceNextInner(counter, subscriber);
    })
    .concat_map([](Inner inner) { return inner; })
    .subscribe([&result](Result entry) {result = std::max(result, entry.recursion_); });

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
  EXPECT_NE(0U, MaxRecursionsDuringCountDown(&ProduceNextUsingBunnyHopping)) << "Bunny hopping causes recursive calls";
  // TODO: demonstrate recursive calls when using .tap to schedule followup
  EXPECT_EQ(0U, MaxRecursionsDuringCountDown(&ProduceNextUsingRxSubsequently)) << "RxSubsequently shouldn't cause recursive calls";
}
