#pragma once

#include <pep/async/FakeVoid.hpp>
#include <pep/async/OnAsio.hpp>
#include <pep/utils/Shared.hpp>

#include <rxcpp/rx-observable.hpp>
#include <rxcpp/operators/rx-timeout.hpp>
#include <optional>

namespace pep {

using RxAsioDuration = std::chrono::milliseconds;

/*! \brief Creates an observable that produces a single item after a specified amount of time.
 * \param duration The time after which the item is emitted.
 * \param io_context The I/O context on which the timer will be scheduled.
 * \param observe_on The coordination to use for (on_next, on_error, on_completed) notifications.
 * \return An observable producing a single value after (the observable is subscribed and) the specified
 *         duration has passed.
 *
 * \remark Replacement for rxcpp's own timer() source, which doesn't play well with our ASIO coordination.
 *         Consider e.g.
 *           rxcpp::observable<>::timer(std::chrono::hours(1))
 *             .timeout(std::chrono::seconds(5))
 *         When the timeout expires after 5 seconds, the timer source should be cancelled.
 *         But cancellation is not supported by our Boost ASIO coordination, so the implementing
 *         Boost steady_timer will keep running for an hour. During that time, the associated
 *         work won't be removed from the io_context, a.o. needlessly eating resources and
 *         preventing the io_context::run() method from terminating.
 *         This function produces a timer observable that _does_ cancel pending ASIO work when
 *         it's unsubscribed. Note that the observable emits a FakeVoid, while rxcpp's timer() source
 *         emits "an integer". See https://reactivex.io/RxCpp/rx-timer_8hpp.html .
 */
rxcpp::observable<FakeVoid> RxAsioTimer(const RxAsioDuration& duration, boost::asio::io_context& io_context, rxcpp::observe_on_one_worker observe_on);


/*! \brief Produces an rxcpp::timeout_error if a source observable doesn't terminate within a specified time frame.
 *
 * \remark Frontend/replacement for rxcpp's own .timeout() method, which
 *         - doesn't play well with our ASIO coordination: see the RxTimeout.test.cpp source for details.
 *         - applies the timeout to the source observable's first emission instead of its termination.
 */
class RxAsioTimeout {
private:
  RxAsioDuration duration_;
  boost::asio::io_context& ioContext_;
  rxcpp::observe_on_one_worker observeOn_;

public:
  /*! \brief Constructor.
    * \param duration The duration after which the timeout should occur.
    * \param io_context The I/O context that'll host the timeout timer.
    * \param observe_on The coordination to use for (on_next, on_error, on_completed) notifications.
    */
  inline RxAsioTimeout(const RxAsioDuration& duration, boost::asio::io_context& io_context, rxcpp::observe_on_one_worker observe_on)
    : duration_(duration), ioContext_(io_context), observeOn_(observe_on) {
  }

  /*! \brief Applies the timeout to a (source) observable.
    * \tparam TItem The item type emitted by the source observable.
    * \tparam SourceOperator The source operator type included in the observable type.
    * \param items The observable emitting the items.
    * \return An observable producing an rxcpp::timeout_error if the source observable doesn't produce an item in time.
    *
    * \remark The source observable is subscribed to, allowing it to emit items until it terminates or the timeout occurs.
    *         If the timeout occurs before the source observable (fails or) completes, the source observable is unsubscribed
    *         from, allowing it to clean up any resources (e.g. cancelling scheduled events).
    */
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {

    /*! \brief Helper class that coordinates (subscriptions to) the source observable ("items") and a timer observable used to produce an rxcpp::timeout_error.
    */
    class Implementor : public std::enable_shared_from_this<Implementor>, public SharedConstructor<Implementor> {
      friend class SharedConstructor<Implementor>;

    private:
      std::optional<rxcpp::subscriber<TItem>> outerSubscriber_;
      std::optional<rxcpp::subscriber<TItem>> itemsSubscriber_;
      std::optional<rxcpp::subscriber<FakeVoid>> timeoutSubscriber_;

      /*! \brief Ensures that inner (source and timer) observables are unsubscribed from.
        * \return The outer subscriber if an on_error or on_completed notification has not yet been sent to it. Otherwise, std::nullopt.
        */
      std::optional<rxcpp::subscriber<TItem>> terminate() {
        auto result = outerSubscriber_;

        if (result.has_value()) {
          assert(itemsSubscriber_.has_value());
          itemsSubscriber_->unsubscribe(); // Allow the source ("items") observable to clean up its things
          itemsSubscriber_ = std::nullopt;

          assert(timeoutSubscriber_.has_value());
          timeoutSubscriber_->unsubscribe(); // Cancel the timer used to produce the timeout
          timeoutSubscriber_ = std::nullopt;

          outerSubscriber_ = std::nullopt;
        }

        return result;
      }

      /*! \brief Invokes the on_next notification on the (outer) subscriber if it hasn't yet received its on_error or on_completed notification.
        * \param item The item to pass to the (outer) subscriber's on_next handler.
        */
      void on_next(const TItem& item) {
        if (outerSubscriber_.has_value()) {
          outerSubscriber_->on_next(item);
        }
      }

      /*! \brief Invokes the on_error notification on the (outer) subscriber if it hasn't yet received its on_error or on_completed notification.
        * \param exception The exception to pass to the (outer) subscriber's on_error handler.
        */
      void on_error(std::exception_ptr exception) {
        auto notify = this->terminate();
        if (notify.has_value()) {
          notify->on_error(exception);
        }
      }

      /*! \brief Invokes the on_completed notification on the (outer) subscriber if it hasn't yet received its on_error or on_completed notification.
        */
      void on_completed() {
        auto notify = this->terminate();
        if (notify.has_value()) {
          notify->on_completed();
        }
      }

      /*! \brief Constructor.
        * \param subscriber The (outer) subscriber that will receive (on_next, on_error, on_completed) notifications.
        */
      explicit Implementor(rxcpp::subscriber<TItem> subscriber)
        : outerSubscriber_(subscriber) {
      }

    public:
      /*! \brief Subscribes to a source observable and lets it emit items to the (outer) subscriber as long as a timeout doesn't occur.
        * \param items The source observable producing items.
        * \param timeout_after The time after which an rxcpp::timeout_error should occur if the source observable hasn't terminated yet.
        * \param io_context The I/O context that'll host the timeout timer.
        * \param observe_on The coordination to use for (on_next, on_error, on_completed) notifications.
        */
      void process(rxcpp::observable<TItem, SourceOperator> items, RxAsioDuration timeout_after, boost::asio::io_context& io_context, rxcpp::observe_on_one_worker observe_on) {
        assert(outerSubscriber_.has_value());
        assert(!itemsSubscriber_.has_value());
        assert(!timeoutSubscriber_.has_value());

        auto self = pep::SharedFrom(*this);

        // Create (and store) a subscriber that'll forward notifications from the source ("items") observable to the (outer) subscriber
        itemsSubscriber_ = rxcpp::make_subscriber<TItem>(
          [self](const TItem& item) {self->on_next(item); },
          [self](std::exception_ptr exception) {self->on_error(exception); },
          [self]() {self->on_completed(); }
        );
        // Attach our forwarding subscriber to the source ("items") observable
        items
          .subscribe_on(pep::ObserveOnAsio(io_context)) // Schedule the source observable on our I/O context (preventing it from e.g. blocking this thread)...
          .observe_on(observe_on) // ...but ensure that notifications are serviced using the caller-supplied coordination
          .subscribe(*itemsSubscriber_);

        // Create (and store) a subscriber that'll produce a timeout_error when a timer elapses
        timeoutSubscriber_ = rxcpp::make_subscriber<FakeVoid>(
          [self](FakeVoid) {self->on_error(std::make_exception_ptr(rxcpp::timeout_error("Timeout occurred"))); },
          [self](std::exception_ptr exception) {self->on_error(exception); },
          []() { /* ignore: we already invoked self->on_error in the on_next */ }
        );
        // Attach our exception-raising subscriber to a timer observable
        pep::RxAsioTimer(timeout_after, io_context, observe_on)
          .subscribe(*timeoutSubscriber_);
      }
    };

    // Wait for a(n outer) subscriber before subscribing to the source ("items") observable and starting the timeout
    return CreateObservable<TItem>([items, duration = duration_, &io_context = ioContext_, observe_on = observeOn_](rxcpp::subscriber<TItem> subscriber) {
      Implementor::Create(subscriber)->process(items, duration, io_context, observe_on);
      });
  }

};


}
