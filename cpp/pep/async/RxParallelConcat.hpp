#pragma once

#include <cassert>
#include <optional>

#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-concat.hpp>

namespace pep
{
// RxParallelConcat -- like concat, but parallelised
//
// Usage:  rxcpp::observable<rxcpp::observable<T>> values;
//         values.op(RxParallelConcat(5))  // instead of values.concat()
//                                         // for 5 parallel subscriptions
//
// The observable values.op(RxParallelConcat(max_subscriptions))
// behaves identically to values.concat().  The difference is in
// how the source observable, values, is consumed.  While concat
// subscribes only to one observable at a time, RxParallelConcat
// subscribes to up to max_subscription observables in parallel.
//
// The items emitted by the 'head' observable are immediatily passed on,
// while the items emitted by the others are cached to be emitted later.
//
//
// Possible improvements:
//
//  - turn detail::CachingObservable into a more general construction;
//    abstract away from the difference between emptying the cache
//    and hijacking in adjust().

namespace detail
{

template <typename T>
class CachingSubscriber
{
  std::queue<T> stored_items;
  bool completed = false;
  std::optional<std::exception_ptr> exception;
  bool _empty = false;

public:
  inline void on_next(T t) {
    assert(!this->completed);
    assert(!this->exception.has_value());
    this->stored_items.push(t);
  }

  inline void on_error(const std::exception_ptr& ep) {
    assert(!this->completed);
    assert(!this->exception.has_value());
    this->exception = ep;
  }

  inline void on_completed() {
    assert(!this->completed);
    assert(!this->exception.has_value());
    this->completed = true;
  }

  inline bool item_ready() {
    return !this->stored_items.empty();
  }

  inline bool end_ready() {
    return this->stored_items.empty()
      && (this->completed || this->exception.has_value());
  }

  inline T pop() {
    assert(this->item_ready());
    T result(this->stored_items.front());
    this->stored_items.pop();
    return result;
  }

  template <typename OnNext, typename OnError, typename OnCompleted>
  bool take_one(OnNext on_next, OnError on_error, OnCompleted on_completed) {
    if (this->_empty)
      return false;
    if (this->item_ready()) {
      on_next(this->pop());
      return true;
    }
    if (this->completed) {
      this->_empty = true;
      on_completed();
      return false;
    }
    if (this->exception.has_value()) {
      this->_empty = true;
      on_error(*(this->exception));
      return false;
    }
    return false;
  }
};


template <typename T>
class RxParallelConcatContext
  : public std::enable_shared_from_this<RxParallelConcatContext<T>>
{
  const size_t max_subscriptions;
  const rxcpp::subscriber<T> target;

  bool stopped = false;
  std::shared_ptr<RxParallelConcatContext> keep_this_alive;

  CachingSubscriber<rxcpp::observable<T>> obs_cache;

public:
  struct CachingObservable {
    CachingSubscriber<T> item_cache;
    rxcpp::observable<T> observable;
    std::optional<rxcpp::composite_subscription> subscription;
    std::shared_ptr<rxcpp::subscriber<T>> subscriber;

    CachingObservable(rxcpp::observable<T> observable)
      : item_cache(),
        observable(observable),
        subscription(),
        subscriber()
    {
      //this->subscription.emplace(); // fill optional with default value
      this->subscription = rxcpp::composite_subscription();

      // To prevent having to subscribe and unsubscribe when we wish to change
      // the target of the items of the observable from this cache to
      // something else (via the hijack method), we forward all items
      // to an intermediate subscriber that can be changed.
      this->subscriber = std::make_shared<rxcpp::subscriber<T>>(
            rxcpp::make_subscriber<T>(
          [this](T t){ // on_next
            this->item_cache.on_next(t);
          },
          [this](const std::exception_ptr& ep) { // on_error
            this->item_cache.on_error(ep);
          },
          [this]() { // on_complete
            this->item_cache.on_completed();
          }));

      // Since the following subscription of this->observable might
      // outlive this CachingObservable, we must be careful not to
      // capture 'this'.
      this->observable.subscribe(*(this->subscription),
          [subscriber=this->subscriber](T t){ // on_next
            subscriber->on_next(t);
          },
          [subscriber=this->subscriber](const std::exception_ptr& ep) {
            // on_error
            subscriber->on_error(ep);
          },
          [subscriber=this->subscriber]() { // on_completed
            subscriber->on_completed();
          });
    }

    ~CachingObservable() {
      if (this->subscription.has_value()) {
        if (this->subscription->is_subscribed()) {
          this->subscription->unsubscribe();
        }
      }
    }

    // redirect the items from this->observable to the specified subscriber
    rxcpp::composite_subscription hijack(rxcpp::subscriber<T> new_subscriber) {
      assert(this->subscription.has_value());
      rxcpp::composite_subscription result(*(this->subscription));
      this->subscription.reset();
      *(this->subscriber) = new_subscriber;
      return result;
    }
  };

  using caching_observable = std::unique_ptr<CachingObservable>;

private:

  std::queue<caching_observable> caching;

  std::optional<rxcpp::composite_subscription> head;

  void inline clear_head() {
    assert(this->head.has_value());
    if (this->head->is_subscribed())
      this->head->unsubscribe();
    this->head.reset();
  }

  // Make sure nothing more is written to target
  //
  // WARNING: this method resets the keep_this_alive member, so
  // if 'this' is not kept alive in some other way, it might be destroyed
  // immediately after stop() returns.
  //
  // adjust() solves this by having a keep_this_alive local.
  void stop() {
    assert(!this->stopped);
    if(this->head.has_value())
      this->clear_head();

    while (!this->caching.empty()) {
      this->caching.pop(); // the CachingObservable will destroy itself
    }

    // we can leave this->obs_cache as it is;
    // we can't stop the subscription filling it anyhow

    this->stopped = true;

    this->keep_this_alive.reset();
  }

  // sets this->head if necessary and possible; returns whether it was
  inline bool adjust_head() {
    assert(!this->stopped);

    bool did_something = false;

    if (this->head.has_value())
      return did_something;

    if (this->caching.empty())
      return did_something;

    while (!this->caching.empty()) {
      caching_observable co (std::move(this->caching.front()));
      this->caching.pop();
      did_something = true;

      // empty co's cache first, before adjusting the subscription;
      // sending items to this->target might make co->observable produce more
      // items.
      bool completed = false;
      bool error = false;
      while (co->item_cache.take_one( // guaranteed to be blocking!
        [this](T t){ // on_next
          assert(!this->stopped);
          this->target.on_next(t);
        },
        [&](const std::exception_ptr& ep){ // on_error
          assert(!this->stopped);
          assert(co->subscription.has_value());
          assert(!co->subscription->is_subscribed());
          this->stop();
          // 'this' is being kept alive by the keep_this_alive local of adjust()
          this->target.on_error(ep);
          error = true;
        },
        [&](){ // on_complete
          assert(!this->stopped);
          assert(co->subscription.has_value());
          assert(!co->subscription->is_subscribed());
          completed = true;
        }
      ));

      if (error)
        break;

      if (completed)
        continue;

      this->head = co->hijack(rxcpp::make_subscriber<T>(
        [this](T t){ // on_next
          assert(!this->stopped);
          this->target.on_next(t);
        },
        [this](const std::exception_ptr& ep){ // on_error
          auto keep_this_alive = this->shared_from_this();
          this->stop();
          this->target.on_error(ep);
        },
        [this](){ // on_complete
          assert(!this->stopped);
          this->clear_head();
          this->adjust();
          // WARNING: 'this' might be destroyed here
        }
      ));
      break;
    }

    return did_something;
  }

  // moves observables from this->obs_cache to this->caching if necessary
  // and possible; returns if it was.
  inline bool adjust_caching() {
    assert(!this->stopped);

    bool did_something = false;
    while ( (this->caching.size() + 1 < this->max_subscriptions)
        && this->obs_cache.item_ready()) {

      this->caching.emplace(new CachingObservable(this->obs_cache.pop()));

      did_something = true;
    }

    return did_something;
  }

  // adjust this->head and this->caching;
  // returns whether something was changed.
  inline bool adjust_one_pass() {
    bool did_something = this->adjust_head();
    if (!this->stopped)
      did_something |= this->adjust_caching();
    return did_something;
  }

  // WARNING: after this method has finished, 'this' might be destroyed!
  // (Indeed, adjust can call stop which resets this->keep_this_alive,
  //  which might be the only thing keeping 'this' alive.)
  void adjust() {
    assert(!this->stopped);

    std::shared_ptr<RxParallelConcatContext> keep_this_alive
      = this->shared_from_this();

    while (this->adjust_one_pass())
      if (this->stopped)
        return;

    // check if we are completely done
    if (this->head.has_value()
        || !this->caching.empty()
        || !this->obs_cache.end_ready()) {
      // we're not done yet
      return;
    }

    this->obs_cache.take_one(
      [](rxcpp::observable<T> obs) { // on_next
        assert(false);  // we know this->obs_cache.end_ready()
      },
      [this](const std::exception_ptr ep) { // on_error
        this->stop();
        // this is being kept alive by the keep_this_alive local of adjust()
        this->target.on_error(ep);
      },
      [this]() {
        this->stop();
        // this is being kept alive by the keep_this_alive local of adjust()
        this->target.on_completed();
      }
    );
  }

  void enable_keep_alive() {
    // We're being kept alive by the subscription we received;
    // once it has finised, we keep ourselves alive with a shared_ptr.
    assert(!this->keep_this_alive);
    this->keep_this_alive = this->shared_from_this();
  }

public:
  inline void handle_on_next(rxcpp::observable<T> obs) {
    // We might already be stopped by an error in one of the subobservables.
    if (this->stopped)
      return;

    this->obs_cache.on_next(obs);
    this->adjust();
  }

  inline void handle_on_error(const std::exception_ptr& ep) {
    // We might already be stopped by an error in one of the subobservables.
    if (this->stopped)
      return;

    this->enable_keep_alive();
    this->obs_cache.on_error(ep);
    this->adjust();
    // WARNING: this might be destroyed here
  }

  inline void handle_on_completed() {
    // We might already be stopped by an error in one of the subobservables.
    if (this->stopped)
      return;

    this->enable_keep_alive();
    this->obs_cache.on_completed();
    this->adjust();
    // WARNING: this might be destroyed here
  }

  RxParallelConcatContext(
      size_t max_subscriptions,
      const rxcpp::subscriber<T> target)
    : max_subscriptions(max_subscriptions),
      target(target),
      stopped(false),
      keep_this_alive(), // this->shared_from_this() is not available yet
      obs_cache(),
      caching(),
      head()
  {
  }
};


// Instances of this class are made to be passed to
//   rxcpp::observable<...>.lift(   )
template <typename T>
class RxParallelConcat_on_subscribers
{
  const size_t max_subscriptions;

public:
  rxcpp::subscriber<rxcpp::observable<T>> operator() (
      rxcpp::subscriber<T> target) const {

    auto context = std::make_shared<RxParallelConcatContext<T>>(
        max_subscriptions, target);

    return rxcpp::make_subscriber<rxcpp::observable<T>>(
        [context](rxcpp::observable<T> obs){
          context->handle_on_next(obs);
        },
        [context](const std::exception_ptr & e){
          context->handle_on_error(e);
        },
        [context](){
          context->handle_on_completed();
        }
    );
  }

  RxParallelConcat_on_subscribers(size_t max_subscriptions)
    : max_subscriptions(max_subscriptions) {

      // If max_subscriptions==1, then you should have used the regular concat.
      // Setting max_subscriptions=0 does not make any sense.
      assert(max_subscriptions > 1);
  }
};

} // namespace detail

class RxParallelConcat {
  const size_t max_subscriptions;

public:
  RxParallelConcat(size_t max_subscriptions)
    : max_subscriptions(max_subscriptions)
  {}

  template <typename T, typename Source1, typename Source2>
  rxcpp::observable<T> operator() (
      rxcpp::observable<rxcpp::observable<T,Source1>,Source2> obs) const {

    if (max_subscriptions==1)
      return obs.concat();

    return obs.template lift<T>(
        detail::RxParallelConcat_on_subscribers<T>(this->max_subscriptions))
              .as_dynamic();
  }
};

}

