#include <pep/async/RxSubsequently.hpp>
#include <pep/utils/Defer.hpp>
#include <boost/algorithm/string/case_conv.hpp>
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

// Helper class to track down linted memory leak: see #2862
class InstanceCountedCallback {
private:
  using Raw = std::function<void()>;
  Raw raw_;

  static size_t instances;

public:
  explicit InstanceCountedCallback(Raw raw) : raw_(std::move(raw)) { ++instances; }

  InstanceCountedCallback(const InstanceCountedCallback& other) : raw_(other.raw_) { ++instances; }
  InstanceCountedCallback(InstanceCountedCallback&& other) noexcept : raw_(std::move(other.raw_)) { ++instances; }
  ~InstanceCountedCallback() noexcept { --instances; }

  void operator()() const { raw_(); }

  static size_t InstanceCount() { return instances; }

};

size_t InstanceCountedCallback::instances = 0U;

Inner MakeInner(std::shared_ptr<unsigned> counter, InstanceCountedCallback andInvoke = InstanceCountedCallback([]() {})) {
  assert(*counter != 0U);

  // See #2862: as demonstrated by means of our InstanceCountedCallback, this is a false positive
  //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
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

  auto callbackInstancesBefore = InstanceCountedCallback::InstanceCount();

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

  // Verify that this approach doesn't leak callback instances as reported/suspected by linter: see #2862
  EXPECT_EQ(callbackInstancesBefore, InstanceCountedCallback::InstanceCount()) << "Callback leaked by " << boost::algorithm::to_lower_copy(std::string(description));
}

void ProduceNextUsingBunnyHopping(std::shared_ptr<unsigned> counter, rxcpp::subscriber<Inner> subscriber) {
  if (*counter != 0U) {
    subscriber.on_next(MakeInner(counter, InstanceCountedCallback([counter, subscriber]() {ProduceNextUsingBunnyHopping(counter, subscriber); })));
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
        InstanceCountedCallback([counter, subscriber]() {ProduceNextUsingRxNativeTap(counter, subscriber); })
      ));
  } else {
    subscriber.on_completed();
  }
}

void ProduceNextUsingRxSubsequently(std::shared_ptr<unsigned> counter, rxcpp::subscriber<Inner> subscriber) {
  if (*counter != 0U) {
    subscriber.on_next(MakeInner(counter)
      .op(pep::RxSubsequently(InstanceCountedCallback([counter, subscriber]() {ProduceNextUsingRxSubsequently(counter, subscriber); }))));
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

  // Verify that our unit test doesn't leak callback instances as reported/suspected by linter: see #2862
  EXPECT_EQ(0U, InstanceCountedCallback::InstanceCount()) << "Callback was leaked by at least one test case";
}
