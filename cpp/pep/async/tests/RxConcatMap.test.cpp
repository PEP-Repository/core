#include <gtest/gtest.h>

#include <pep/async/CreateObservable.hpp>
#include <pep/async/RxConcatMap.hpp>
#include <pep/utils/CollectionUtils.hpp>

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>
#include <rxcpp/operators/rx-take.hpp>

using namespace pep;
using namespace std::ranges;

namespace {

/// An observable that emits everything during subscribe(), like a replaying RxCache.
rxcpp::observable<int> Synchronous(std::vector<int> values) {
  return CreateObservable<int>([values = std::move(values)](rxcpp::subscriber<int> subscriber) {
    for (int value : values) {
      if (!subscriber.is_subscribed()) {
        return;
      }
      subscriber.on_next(value);
    }
    subscriber.on_completed();
  });
}

TEST(RxConcatMap, EmitsInSourceOrder) {
  std::vector<int> received;
  rxcpp::sources::range(0, 4-1)
    .op(RxConcatMap([](int i) { return Synchronous({i * 10, i * 10 + 1}); }))
    .subscribe([&received](int i) { received.push_back(i); }, [](std::exception_ptr) {});

  const std::vector<int> expected{0, 1, 10, 11, 20, 21, 30, 31};
  EXPECT_EQ(received, expected);
}

TEST(RxConcatMap, CompletesOnEmptySource) {
  bool completed = false;
  size_t emitted = 0;
  rxcpp::observable<>::empty<int>()
    .op(RxConcatMap([](int i) { return Synchronous({i}); }))
    .subscribe([&emitted](int) { ++emitted; }, [](std::exception_ptr) {}, [&completed] { completed = true; });

  EXPECT_TRUE(completed);
  EXPECT_EQ(emitted, 0U);
}

TEST(RxConcatMap, SkipsEmptyInnerObservables) {
  std::vector<int> received;
  bool completed = false;
  rxcpp::sources::range(0, 4-1)
    .op(RxConcatMap([](int i) -> rxcpp::observable<int> {
      if (i % 2 == 0) {
        return rxcpp::observable<>::empty<int>();
      }
      return Synchronous({i});
    }))
    .subscribe([&received](int i) { received.push_back(i); }, [](std::exception_ptr) {}, [&completed] { completed = true; });

  const std::vector<int> expected{1, 3};
  EXPECT_EQ(received, expected);
  EXPECT_TRUE(completed);
}

TEST(RxConcatMap, ForwardsSourceError) {
  bool errored = false;
  bool completed = false;
  rxcpp::observable<>::error<int>(std::make_exception_ptr(std::runtime_error("source")))
    .op(RxConcatMap([](int i) { return Synchronous({i}); }))
    .subscribe([](int) {}, [&errored](std::exception_ptr) { errored = true; }, [&completed] { completed = true; });

  EXPECT_TRUE(errored);
  EXPECT_FALSE(completed);
}

TEST(RxConcatMap, ForwardsInnerError) {
  std::vector<int> received;
  bool errored = false;
  rxcpp::sources::range(0, 4-1)
    .op(RxConcatMap([](int i) -> rxcpp::observable<int> {
      if (i == 2) {
        return rxcpp::observable<>::error<int>(std::make_exception_ptr(std::runtime_error("inner")));
      }
      return Synchronous({i});
    }))
    .subscribe([&received](int i) { received.push_back(i); }, [&errored](std::exception_ptr) { errored = true; });

  const std::vector<int> expected{0, 1};
  EXPECT_EQ(received, expected);
  EXPECT_TRUE(errored);
}

TEST(RxConcatMap, ForwardsSelectorException) {
  bool errored = false;
  std::string message;
  rxcpp::sources::range(0, 4-1)
    .op(RxConcatMap([](int i) -> rxcpp::observable<int> {
      if (i == 1) {
        throw std::runtime_error("selector");
      }
      return Synchronous({i});
    }))
    .subscribe([](int) {}, [&errored, &message](std::exception_ptr ep) {
      errored = true;
      try {
        std::rethrow_exception(ep);
      }
      catch (const std::exception& error) {
        message = error.what();
      }
    });

  EXPECT_TRUE(errored);
  EXPECT_EQ(message, "selector");
}

TEST(RxConcatMap, StopsProducingWhenUnsubscribed) {
  std::vector<int> received;
  rxcpp::sources::range(0, 100-1)
    .op(RxConcatMap([](int i) { return Synchronous({i}); }))
    .take(3)
    .subscribe([&received](int i) { received.push_back(i); }, [](std::exception_ptr) {});

  const std::vector<int> expected{0, 1, 2};
  EXPECT_EQ(received, expected);
}

/// Emits \p value and completes, but only after \p hops trips through the io_context.
void PostSteps(boost::asio::io_context& io, rxcpp::subscriber<int> subscriber, int value, int hops) {
  if (hops == 0) {
    subscriber.on_next(value);
    subscriber.on_completed();
    return;
  }
  post(io, [&io, subscriber, value, hops] { PostSteps(io, subscriber, value, hops - 1); });
}

/// An observable that never completes during subscribe(), like a pending Castor request.
rxcpp::observable<int> Delayed(boost::asio::io_context& io, int value, int hops) {
  return CreateObservable<int>([&io, value, hops](rxcpp::subscriber<int> subscriber) {
    PostSteps(io, subscriber, value, hops);
  });
}

/// Unsubscribing must also stop us from subscribing to further *asynchronous* inner observables.
TEST(RxConcatMap, StopsSubscribingToAsynchronousInnersWhenUnsubscribed) {
  boost::asio::io_context io;
  int selectorInvocations = 0;
  std::vector<int> received;

  rxcpp::sources::range(0, 100-1)
    .op(RxConcatMap([&io, &selectorInvocations](int i) {
      ++selectorInvocations;
      return Delayed(io, i, 1);
    }))
    .take(3)
    .subscribe([&received](int i) { received.push_back(i); }, [](std::exception_ptr) {});
  io.run();

  const std::vector<int> expected{0, 1, 2};
  EXPECT_EQ(received, expected);
  EXPECT_EQ(selectorInvocations, 3); // The queued remainder is abandoned rather than worked through
}


/// Counts how many inner observables hold a resource at the same time.
class LiveCounter {
  size_t live_ = 0;
  size_t peak_ = 0;

public:
  void acquire() {
    ++live_;
    peak_ = std::max(peak_, live_);
  }

  void release() { --live_; }

  size_t getPeak() const { return peak_; }
};

/// Registers itself with a LiveCounter for as long as it exists.
class InnerResource {
  LiveCounter& counter_;

public:
  explicit InnerResource(LiveCounter& counter) : counter_(counter) { counter_.acquire(); }
  ~InnerResource() noexcept { counter_.release(); }
  InnerResource(const InnerResource&) = delete;
  InnerResource& operator=(const InnerResource&) = delete;
};

/// \brief Completes during subscribe(), holding its resource only while that frame runs.
/// \details A recursive drain never lets the frame return, so the resources pile up.
rxcpp::observable<int> HeldWhileSubscribing(LiveCounter& counter, boost::asio::io_context&, int value) {
  return CreateObservable<int>([&counter, value](rxcpp::subscriber<int> subscriber) {
    const InnerResource resource(counter);
    subscriber.on_next(value);
    subscriber.on_completed(); // Recurses for rxcpp concat_map
  });
}

/// \brief Completes asynchronously from a later io_context handler, holding its resource until unsubscribed.
/// \details The peak then shows whether each inner observable is released before the next one
/// is subscribed to, which a frame-held resource cannot show: any non-recursive implementation
/// frees that one in time regardless.
rxcpp::observable<int> HeldUntilUnsubscribed(LiveCounter& counter, boost::asio::io_context& io, int value) {
  return CreateObservable<int>([&counter, &io, value](rxcpp::subscriber<int> subscriber) {
    auto resource = std::make_shared<InnerResource>(counter);
    subscriber.add(rxcpp::make_subscription([resource] {}));
    PostSteps(io, subscriber, value, 1);
  });
}

constexpr auto PepConcatMap = [](auto source, auto selector) { return source.op(RxConcatMap(selector)); };
constexpr auto RxcppConcatMap = [](auto source, auto selector) { return source.concat_map(selector).as_dynamic(); };

struct RunResult {
  std::vector<int> received;
  size_t peakLiveResources;
};

/// \brief Runs \p count items through \p concatMap, building inner observables with \p makeInner.
/// \details Inner observable 0 completes asynchronously rather than during
/// subscribe(), so the source runs to completion first and the remaining items are queued:
/// the backlog whose draining is what rxcpp's concat_map does recursively.
/// \param count The number of source items.
/// \param concatMap Applies the operator under test to the source observable.
/// \param makeInner Produces the inner observable for every item but the first.
RunResult RunPipeline(int count, auto concatMap,
    std::function<rxcpp::observable<int>(LiveCounter& counter, boost::asio::io_context& io, int value)> makeInner) {
  LiveCounter counter;
  boost::asio::io_context io;
  std::vector<int> received;

  concatMap(rxcpp::sources::range(0, count-1), [&counter, &io, makeInner](int i) -> rxcpp::observable<int> {
    if (i == 0) {
      return Delayed(io, i, 1);
    }
    return makeInner(counter, io, i);
  }).subscribe([&received](int i) { received.push_back(i); }, [](std::exception_ptr) {});

  io.run();
  return {std::move(received), counter.getPeak()};
}

TEST(RxConcatMap, EmitsSameItemsAsRxcppForDrainedBacklog) {
  const RunResult ours = RunPipeline(50, PepConcatMap, HeldWhileSubscribing);
  const RunResult theirs = RunPipeline(50, RxcppConcatMap, HeldWhileSubscribing);

  EXPECT_EQ(ours.received, theirs.received);
  EXPECT_EQ(ours.received, RangeToVector(views::iota(0, 50)));
}

/// \brief The reason this operator exists: a backlog must drain without recursion.
/// \details Measured as resources held by inner subscribe frames rather than as a stack size,
/// which is not stable enough to compare exactly.
TEST(RxConcatMap, DrainsBacklogWithoutAccumulatingInnerResources) {
  EXPECT_EQ(RunPipeline(10, PepConcatMap, HeldWhileSubscribing).peakLiveResources, 1U);
  EXPECT_EQ(RunPipeline(1000, PepConcatMap, HeldWhileSubscribing).peakLiveResources, 1U);
}

/// \brief Guards the assumption above: rxcpp's concat_map is what this operator works around.
/// \details Its recursion holds every queued inner observable's subscribe frame on the stack
/// until the last one finishes. If our rxcpp fork is ever patched to drain iteratively, this
/// test starts failing, which is the signal that RxConcatMap can be retired.
TEST(RxConcatMap, RxcppConcatMapAccumulatesInnerResourcesWhileDrainingABacklog) {
  EXPECT_EQ(RunPipeline(10, RxcppConcatMap, HeldWhileSubscribing).peakLiveResources, 9U);
  EXPECT_EQ(RunPipeline(1000, RxcppConcatMap, HeldWhileSubscribing).peakLiveResources, 999U);
}

/// \brief An inner observable's resources are released before the next one is subscribed to.
/// \details Only asynchronous inner observables test this: with a synchronous backlog the
/// drain loop's re-entrancy guard already lets rxcpp's completeddetacher release each inner in
/// time, so dropping ConcatMapState's mInnerLifetime.unsubscribe() would go unnoticed.
TEST(RxConcatMap, ReleasesInnerResourcesBeforeSubscribingToTheNext) {
  EXPECT_EQ(RunPipeline(200, PepConcatMap, HeldUntilUnsubscribed).peakLiveResources, 1U);
}

/// rxcpp's concat_map unsubscribes before it recurses, so on this axis it behaves identically.
TEST(RxConcatMap, RxcppConcatMapAlsoReleasesInnerResourcesBeforeSubscribingToTheNext) {
  EXPECT_EQ(RunPipeline(200, RxcppConcatMap, HeldUntilUnsubscribed).peakLiveResources, 1U);
}


/// \brief Inner observables completing on an io_context must still be emitted in source order.
///
/// Earlier items are given *more* io_context hops than later ones, so an operator that
/// subscribed to its inner observables eagerly would emit them roughly in reverse. That
/// gradient is what gives this test teeth: with a uniform number of hops even a merging
/// operator keeps the items in order, so the assertion would hold for the wrong reasons.
///
/// What this covers that the test below does not is the path through advance() where *no*
/// inner observable ever completes during subscribe(): every item leaves the drain loop and is
/// resumed by a later io_context handler, rather than being drained by one running loop.
TEST(RxConcatMap, PreservesOrderWithAsynchronousInners) {
  constexpr int Count = 20;
  boost::asio::io_context io;
  std::vector<int> received;
  bool completed = false;

  rxcpp::sources::range(0, Count-1)
    .op(RxConcatMap([&io](int i) { return Delayed(io, i, Count - i); }))
    .subscribe([&received](int i) { received.push_back(i); },
               [](std::exception_ptr) {},
               [&completed] { completed = true; });

  io.run();

  EXPECT_TRUE(completed);
  EXPECT_EQ(received, RangeToVector(views::iota(0, Count)));
}

/// Mixing a slow first inner with immediately completing ones is the pullcastor shape.
TEST(RxConcatMap, PreservesOrderWhenBacklogDrainsAfterAsynchronousInner) {
  boost::asio::io_context io;
  std::vector<int> received;
  bool completed = false;

  rxcpp::sources::range(0, 50-1)
    .op(RxConcatMap([&io](int i) -> rxcpp::observable<int> {
      if (i != 0) {
        return Synchronous({i});
      }
      return Delayed(io, i, 1);
    }))
    .subscribe([&received](int i) { received.push_back(i); },
               [](std::exception_ptr) {},
               [&completed] { completed = true; });
  io.run();

  EXPECT_TRUE(completed);
  EXPECT_EQ(received, RangeToVector(views::iota(0, 50)));
}

} // namespace
