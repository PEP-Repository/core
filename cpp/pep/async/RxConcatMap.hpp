#pragma once

#include <pep/utils/Shared.hpp>

#include <deque>
#include <exception>
#include <optional>
#include <utility>

#include <rxcpp/rx-lite.hpp>

namespace pep {

namespace detail {

/// \brief Subscription state shared by the source and inner subscribers of an RxConcatMap.
///
/// \details Mirrors rxcpp's concat_map, with the one difference that is the point of this
/// class: rxcpp advances to the next queued item by calling subscribe_to() from inside the
/// previous inner observable's on_completed handler, so a backlog of synchronously completing
/// inner observables is drained recursively, one nesting level per item. This class advances
/// from a loop instead, and a re-entrant advance() just returns and lets that loop continue.
///
/// \tparam TSource The item type of the source observable.
/// \tparam TResult The item type of the inner observables, and of the resulting observable.
/// \tparam Selector The type of the function producing an inner observable from a source item.
template <typename TSource, typename TResult, typename Selector>
class ConcatMapState : public std::enable_shared_from_this<ConcatMapState<TSource, TResult, Selector>>,
                       public SharedConstructor<ConcatMapState<TSource, TResult, Selector>> {
  friend SharedConstructor<ConcatMapState>;

private:
  Selector mSelector;
  rxcpp::subscriber<TResult> mTarget;
  std::deque<TSource> mQueue;
  rxcpp::composite_subscription mInnerLifetime = rxcpp::composite_subscription::empty();
  bool mInnerSubscribed = false;
  bool mSourceCompleted = false;
  bool mAdvancing = false;
  bool mTerminated = false;

  ConcatMapState(Selector selector, rxcpp::subscriber<TResult> target)
    : mSelector(std::move(selector)), mTarget(std::move(target)) {
  }

  /// Subscribes to the inner observable for a single source item.
  void subscribeToInner(TSource value) {
    /* Held in an optional rather than an rxcpp::observable<TResult> so that the inner
     * observable keeps its concrete type: type-erasing it would cost an allocation and a
     * virtual dispatch per source item. The optional only exists because the selector call
     * needs to be guarded separately from the subscribe call below.
     */
    using TCollection = decltype(std::declval<const Selector&>()(std::declval<TSource>()));
    std::optional<TCollection> inner;
    try {
      inner.emplace(mSelector(std::move(value)));
    }
    catch (...) {
      this->terminateWithError(std::current_exception());
      return;
    }

    auto self = SharedFrom(*this);
    mInnerSubscribed = true;
    mInnerLifetime = rxcpp::composite_subscription();

    // Unsubscribing the target must tear down whichever inner observable is running.
    auto token = mTarget.add(mInnerLifetime);
    mInnerLifetime.add(rxcpp::make_subscription([self, token] { self->mTarget.remove(token); }));

    inner->subscribe(rxcpp::make_subscriber<TResult>(
      mInnerLifetime,
      [self](TResult item) { self->mTarget.on_next(std::move(item)); },
      [self](std::exception_ptr error) {
        self->mInnerSubscribed = false;
        self->terminateWithError(error);
      },
      [self] {
        self->mInnerSubscribed = false;
        /* Release this inner observable's resources before the next one is subscribed to.
         * rxcpp cannot do this: its completeddetacher only unsubscribes after the downstream
         * on_completed call returns, and in rxcpp that is the call that recurses.
         */
        self->mInnerLifetime.unsubscribe();
        self->advance();
      }));
  }

  void terminateWithError(std::exception_ptr error) {
    if (mTerminated) {
      return;
    }
    mTerminated = true;
    mQueue.clear();
    mTarget.on_error(error);
  }

public:
  /// \brief Subscribes to queued source items until one of them does not complete immediately.
  /// \details Safe to call re-entrantly: a nested call returns without doing anything, leaving
  /// the work to the loop that is already running.
  void advance() {
    if (mAdvancing || mTerminated) {
      return;
    }

    mAdvancing = true;
    while (!mInnerSubscribed && mTarget.is_subscribed() && !mTerminated) {
      if (!mQueue.empty()) {
        TSource value = std::move(mQueue.front());
        mQueue.pop_front();
        this->subscribeToInner(std::move(value)); // May re-enter advance(), which will return immediately
      }
      else if (mSourceCompleted) {
        mTerminated = true;
        mAdvancing = false;
        mTarget.on_completed();
        return;
      }
      else {
        break; // Nothing to do until the source produces another item
      }
    }
    mAdvancing = false;
  }

  void onSourceNext(TSource value) {
    if (mTerminated) {
      return;
    }
    mQueue.push_back(std::move(value));
    this->advance();
  }

  void onSourceError(std::exception_ptr error) { this->terminateWithError(error); }

  void onSourceCompleted() {
    mSourceCompleted = true;
    this->advance();
  }
};

/// Turns a subscriber for the resulting observable into a subscriber for the source observable.
template <typename TSource, typename TResult, typename Selector>
class ConcatMapLifter {
private:
  Selector mSelector;

public:
  explicit ConcatMapLifter(Selector selector) : mSelector(std::move(selector)) {}

  rxcpp::subscriber<TSource> operator()(rxcpp::subscriber<TResult> target) const {
    auto state = ConcatMapState<TSource, TResult, Selector>::Create(mSelector, target);

    /* The source gets a lifetime of its own so that its completion doesn't tear down the
     * target while an inner observable is still running, but it is attached to the target so
     * that unsubscribing the target does stop the source.
     */
    rxcpp::composite_subscription sourceLifetime;
    target.add(sourceLifetime);

    return rxcpp::make_subscriber<TSource>(
      sourceLifetime,
      [state](TSource value) { state->onSourceNext(std::move(value)); },
      [state](std::exception_ptr error) { state->onSourceError(error); },
      [state] { state->onSourceCompleted(); });
  }
};

} // namespace detail

/// \brief Like rxcpp's concat_map, but without stack recursion proportional to the item count.
///
/// \code
///   myObservable.op(RxConcatMap([](Thing thing) { return GetPartsOf(thing); }))
/// \endcode
///
/// \details Emits the items of the observable that \p selector produces for each source item,
/// in source order, subscribing to one inner observable at a time. rxcpp's concat_map promises
/// the same, but implements "advance to the next item" as a call from inside the previous inner
/// observable's on_completed handler. When source items arrive faster than the inner observables
/// complete -- e.g. an in-memory source feeding a pipeline whose first item needs I/O -- rxcpp
/// queues the backlog and then drains it recursively, using one nesting level per item and
/// keeping every inner observable's data alive until the last one is done. See
/// cpp/pep/apps/RxStackDepth.cpp for the measurements.
///
/// Differences from rxcpp's concat_map:
/// - Only the CollectionSelector form is supported. rxcpp also accepts a ResultSelector and a
///   Coordination; no call site in this repository uses either.
/// - The resulting observable is type-erased with as_dynamic(), as pep's other .op() operators
///   are. That is once per pipeline; inner observables keep their concrete type, so throughput
///   matches rxcpp's concat_map.
/// - Like rxcpp's concat_map without an explicit Coordination, this operator does no locking:
///   it assumes source and inner notifications are serialized, as the Rx contract requires.
///
/// \tparam Selector A function-like type taking a source item and returning an observable.
template <typename Selector>
class RxConcatMap {
private:
  Selector mSelector;

public:
  explicit RxConcatMap(Selector selector) : mSelector(std::move(selector)) {}

  /// \param items The observable emitting the source items.
  /// \return An observable emitting the concatenated items of the inner observables.
  /// \tparam TItem The type of item produced by the source observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  auto operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    using TResult = typename decltype(mSelector(std::declval<TItem>()))::value_type;
    return items.template lift<TResult>(detail::ConcatMapLifter<TItem, TResult, Selector>(mSelector)).as_dynamic();
  }
};

} // namespace pep
