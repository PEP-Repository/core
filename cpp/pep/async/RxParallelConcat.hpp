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
// The observable values.op(RxParallelConcat(maxSubscriptions))
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
  std::queue<T> storedItems_;
  bool completed_ = false;
  std::optional<std::exception_ptr> exception_;
  bool empty_ = false;

public:
  inline void on_next(T t) {
    assert(!completed_);
    assert(!exception_.has_value());
    storedItems_.push(t);
  }

  inline void on_error(const std::exception_ptr& ep) {
    assert(!completed_);
    assert(!exception_.has_value());
    exception_ = ep;
  }

  inline void on_completed() {
    assert(!completed_);
    assert(!exception_.has_value());
    completed_ = true;
  }

  inline bool itemReady() {
    return !storedItems_.empty();
  }

  inline bool endReady() {
    return storedItems_.empty()
      && (completed_ || exception_.has_value());
  }

  inline T pop() {
    assert(this->itemReady());
    T result(storedItems_.front());
    storedItems_.pop();
    return result;
  }

  template <typename OnNext, typename OnError, typename OnCompleted>
  bool takeOne(OnNext on_next, OnError on_error, OnCompleted on_completed) {
    if (empty_)
      return false;
    if (this->itemReady()) {
      on_next(this->pop());
      return true;
    }
    if (completed_) {
      empty_ = true;
      on_completed();
      return false;
    }
    if (exception_.has_value()) {
      empty_ = true;
      on_error(*(exception_));
      return false;
    }
    return false;
  }
};


template <typename T>
class RxParallelConcatContext
  : public std::enable_shared_from_this<RxParallelConcatContext<T>>
{
  const size_t maxSubscriptions_;
  const rxcpp::subscriber<T> target_;

  bool stopped_ = false;
  std::shared_ptr<RxParallelConcatContext> keepThisAlive_;

  CachingSubscriber<rxcpp::observable<T>> obsCache_;

public:
  class CachingObservable;

private:
  std::queue<std::unique_ptr<CachingObservable>> caching_;
  std::optional<rxcpp::composite_subscription> head_;

  void inline clear_head() {
    assert(head_.has_value());
    if (head_->is_subscribed())
      head_->unsubscribe();
    head_.reset();
  }

  // Make sure nothing more is written to target_
  //
  // WARNING: this method resets the keepThisAlive_ member, so
  // if 'this' is not kept alive in some other way, it might be destroyed
  // immediately after stop() returns.
  //
  // adjust() solves this by having a keepThisAlive_ local.
  void stop() {
    assert(!stopped_);
    if(head_.has_value())
      this->clear_head();

    while (!caching_.empty()) {
      caching_.pop(); // the CachingObservable will destroy itself
    }

    // we can leave obsCache_ as it is;
    // we can't stop the subscription filling it anyhow

    stopped_ = true;

    keepThisAlive_.reset();
  }

  // sets head_ if necessary and possible; returns whether it was
  inline bool adjust_head() {
    assert(!stopped_);

    bool did_something = false;

    if (head_.has_value())
      return did_something;

    if (caching_.empty())
      return did_something;

    while (!caching_.empty()) {
      auto co = std::move(caching_.front());
      caching_.pop();
      did_something = true;

      // empty co's cache first, before adjusting the subscription;
      // sending items to target_ might make co->observable produce more
      // items.
      bool completed = false;
      bool error = false;
      while (co->itemCache.takeOne( // guaranteed to be blocking!
        [this](T t){ // on_next
          assert(!stopped_);
          target_.on_next(t);
        },
        [&](const std::exception_ptr& ep){ // on_error
          assert(!stopped_);
          assert(co->hasTerminated());
          this->stop();
          // 'this' is being kept alive by the keepThisAlive_ local of adjust()
          target_.on_error(ep);
          error = true;
        },
        [&](){ // on_complete
          assert(!stopped_);
          assert(co->hasTerminated());
          completed = true;
        }
      ));

      if (error)
        break;

      if (completed)
        continue;

      head_ = co->hijack(rxcpp::make_subscriber<T>(
        [this](T t){ // on_next
          assert(!stopped_);
          target_.on_next(t);
        },
        [this](const std::exception_ptr& ep){ // on_error
          auto keepThisAlive_ = this->shared_from_this();
          this->stop();
          target_.on_error(ep);
        },
        [this](){ // on_complete
          assert(!stopped_);
          this->clear_head();
          this->adjust();
          // WARNING: 'this' might be destroyed here
        }
      ));
      break;
    }

    return did_something;
  }

  // moves observables from obsCache_ to caching_ if necessary
  // and possible; returns if it was.
  inline bool adjust_caching() {
    assert(!stopped_);

    bool did_something = false;
    while ( (caching_.size() + 1 < maxSubscriptions_)
        && obsCache_.itemReady()) {

      caching_.emplace(new CachingObservable(obsCache_.pop()));

      did_something = true;
    }

    return did_something;
  }

  // adjust head_ and caching_;
  // returns whether something was changed.
  inline bool adjust_one_pass() {
    bool did_something = this->adjust_head();
    if (!stopped_)
      did_something |= this->adjust_caching();
    return did_something;
  }

  // WARNING: after this method has finished, 'this' might be destroyed!
  // (Indeed, adjust can call stop which resets keepThisAlive_,
  //  which might be the only thing keeping 'this' alive.)
  void adjust() {
    assert(!stopped_);

    std::shared_ptr<RxParallelConcatContext> keepThisAlive_
      = this->shared_from_this();

    while (this->adjust_one_pass())
      if (stopped_)
        return;

    // check if we are completely done
    if (head_.has_value()
        || !caching_.empty()
        || !obsCache_.endReady()) {
      // we're not done yet
      return;
    }

    obsCache_.takeOne(
      [](rxcpp::observable<T> obs) { // on_next
        assert(false);  // we know obsCache_.endReady()
      },
      [this](const std::exception_ptr ep) { // on_error
        this->stop();
        // this is being kept alive by the keepThisAlive_ local of adjust()
        target_.on_error(ep);
      },
      [this]() {
        this->stop();
        // this is being kept alive by the keepThisAlive_ local of adjust()
        target_.on_completed();
      }
    );
  }

  void enable_keep_alive() {
    // We're being kept alive by the subscription we received;
    // once it has finised, we keep ourselves alive with a shared_ptr.
    assert(!keepThisAlive_);
    keepThisAlive_ = this->shared_from_this();
  }

public:
  inline void handle_on_next(rxcpp::observable<T> obs) {
    // We might already be stopped_ by an error in one of the subobservables.
    if (stopped_)
      return;

    obsCache_.on_next(obs);
    this->adjust();
  }

  inline void handle_on_error(const std::exception_ptr& ep) {
    // We might already be stopped_ by an error in one of the subobservables.
    if (stopped_)
      return;

    this->enable_keep_alive();
    obsCache_.on_error(ep);
    this->adjust();
    // WARNING: this might be destroyed here
  }

  inline void handle_on_completed() {
    // We might already be stopped_ by an error in one of the subobservables.
    if (stopped_)
      return;

    this->enable_keep_alive();
    obsCache_.on_completed();
    this->adjust();
    // WARNING: this might be destroyed here
  }

  RxParallelConcatContext(
      size_t maxSubscriptions_,
      const rxcpp::subscriber<T> target_)
    : maxSubscriptions_(maxSubscriptions_),
      target_(target_),
      stopped_(false),
      keepThisAlive_(), // this->shared_from_this() is not available yet
      obsCache_(),
      caching_(),
      head_()
  {
  }
};

template <typename T>
class RxParallelConcatContext<T>::CachingObservable {
public:
  CachingSubscriber<T> itemCache;

private:
  rxcpp::observable<T> observable_;
  std::optional<rxcpp::composite_subscription> subscription_;
  std::shared_ptr<rxcpp::subscriber<T>> subscriber_;

public:
  CachingObservable(rxcpp::observable<T> observable)
    : itemCache(),
    observable_(observable),
    subscription_(),
    subscriber_() {
    //subscription_.emplace(); // fill optional with default value
    subscription_ = rxcpp::composite_subscription();

    // To prevent having to subscribe and unsubscribe when we wish to change
    // the target_ of the items of the observable from this cache to
    // something else (via the hijack method), we forward all items
    // to an intermediate subscriber that can be changed.
    subscriber_ = std::make_shared<rxcpp::subscriber<T>>(
      rxcpp::make_subscriber<T>(
        [this](T t) { // on_next
          this->itemCache.on_next(t);
        },
        [this](const std::exception_ptr& ep) { // on_error
          this->itemCache.on_error(ep);
        },
        [this]() { // on_complete
          this->itemCache.on_completed();
        }));

    // Since the following subscription of observable_ might
    // outlive this CachingObservable, we must be careful not to
    // capture 'this'.
    observable_.subscribe(*(subscription_),
      [subscriber = subscriber_](T t) { // on_next
        subscriber->on_next(t);
      },
      [subscriber = subscriber_](const std::exception_ptr& ep) {
        // on_error
        subscriber->on_error(ep);
      },
      [subscriber = subscriber_]() { // on_completed
        subscriber->on_completed();
      });
  }

  ~CachingObservable() {
    if (subscription_.has_value()) {
      if (subscription_->is_subscribed()) {
        subscription_->unsubscribe();
      }
    }
  }

  // redirect the items from observable_ to the specified subscriber
  rxcpp::composite_subscription hijack(rxcpp::subscriber<T> new_subscriber) {
    assert(subscription_.has_value());
    rxcpp::composite_subscription result(*(subscription_));
    subscription_.reset();
    *(subscriber_) = new_subscriber;
    return result;
  }

  bool hasSubscription() const {
    return subscription_.has_value();
  }

  bool hasTerminated() const {
    return this->hasSubscription() && !subscription_->is_subscribed();
  }
};


// Instances of this class are made to be passed to
//   rxcpp::observable<...>.lift(   )
template <typename T>
class RxParallelConcat_on_subscribers
{
  const size_t maxSubscriptions_;

public:
  rxcpp::subscriber<rxcpp::observable<T>> operator() (
      rxcpp::subscriber<T> target_) const {

    auto context = std::make_shared<RxParallelConcatContext<T>>(
        maxSubscriptions_, target_);

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

  RxParallelConcat_on_subscribers(size_t maxSubscriptions_)
    : maxSubscriptions_(maxSubscriptions_) {

      // If maxSubscriptions_==1, then you should have used the regular concat.
      // Setting maxSubscriptions_=0 does not make any sense.
      assert(maxSubscriptions_ > 1);
  }
};

} // namespace detail

class RxParallelConcat {
  const size_t maxSubscriptions_;

public:
  RxParallelConcat(size_t maxSubscriptions_)
    : maxSubscriptions_(maxSubscriptions_)
  {}

  template <typename T, typename Source1, typename Source2>
  rxcpp::observable<T> operator() (
      rxcpp::observable<rxcpp::observable<T,Source1>,Source2> obs) const {

    if (maxSubscriptions_==1)
      return obs.concat();

    return obs.template lift<T>(
        detail::RxParallelConcat_on_subscribers<T>(maxSubscriptions_))
              .as_dynamic();
  }
};

}

