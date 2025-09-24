#pragma once

#include <pep/async/CreateObservable.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Shared.hpp>
#include <optional>

namespace pep {

/*! \brief Caches the emissions of an RX observable, allowing them to be (re-)observed locally.
  *
  * Mainly intended to solve timing issues w.r.t. lifetime, an RxCache
  * is kept alive for as long as
  * - the original observable emits items, or
  * - someone observes the cached emissions, or
  * - (obviously) someone retains a shared_ptr to it.
  * (Any condition or combination thereof will keep the object alive.)
  *
  * ONLY USE RxCache with observables that terminate, i.e. they invoke their
  * "onCompleted" handler.
  */
template <typename T>
class RxCache : public std::enable_shared_from_this<RxCache<T>> {
public:
  virtual ~RxCache() noexcept = default;
  virtual rxcpp::observable<T> observe() const = 0;
};

/*! \brief Aggregates the emissions of an observable into (a shared_ptr to) an RxCache instance: CreateRxCache([]() {return myObservable;}).
           The cache does not create a source observable until the cache itself is (observed and) subscribed to.
           If a source observable completes with an error, the cache creates a new source observable when it is re-(observed and )subscribed to.
 * \tparam CreateSource A parameterless function-like type that returns an observable<> instance.
 * \param createSource A function-like object that creates a source observable.
 * \return An RxCache<> emitting the items (or error) emitted by the source observable.
 */
template <typename CreateSource>
std::shared_ptr<RxCache<typename decltype(std::declval<CreateSource>()())::value_type>> CreateRxCache(const CreateSource& createSource);

namespace detail {

// See #1672: this class is named "WaitlessRxCache" because earlier implementations used a WaitGroup and seemed to deadlock because of it.
template <typename T>
class WaitlessRxCache : public RxCache<T> {
  template <typename CreateSource>
  friend std::shared_ptr<RxCache<typename decltype(std::declval<CreateSource>()())::value_type>> pep::CreateRxCache(const CreateSource& createSource);

public:
  using CreateSourceFunction = std::function<rxcpp::observable<T>()>;

private:
  mutable std::optional<CreateSourceFunction> mCreateSource;
  mutable bool mRetrieving = false;
  mutable std::vector<rxcpp::subscriber<T>> mFollowers;
  mutable std::shared_ptr<std::vector<T>> mItems;

private:
  void emitItemsTo(rxcpp::subscriber<T> subscriber) const {
    assert(mItems != nullptr);
    try {
      for (const auto& item : *mItems) {
        subscriber.on_next(item);
      }
      subscriber.on_completed();
    }
    catch (...) {
      subscriber.on_error(std::current_exception());
    }
  }

  void finishRetrieving(std::shared_ptr<std::vector<T>> items) const {
    assert(mRetrieving);
    mRetrieving = false;
    mItems = items;
  }

  void processFollowers() const {
    /* Deal with followers as though they just enlisted after our state change.
     * This will emit the items if we have them, or allow the first follower to become the primary subscriber.
     */
    assert(!mRetrieving);
    auto followers = std::move(mFollowers);
    for (const auto& follower : followers) {
      this->handleSubscriber(follower);
    }
  }

  void startRetrieving(rxcpp::subscriber<T> subscriber) const {
    assert(!mRetrieving);
    mRetrieving = true;

    auto items = std::make_shared<std::vector<T>>();
    auto self = SharedFrom(*this);

    assert(mCreateSource.has_value());
    (*mCreateSource)().subscribe(
      [subscriber, items](const T& item) {
        items->push_back(item); // Collect the item in our local vector...
        subscriber.on_next(item); // ... and let the primary subscriber know about it immediately.
      },
      [subscriber, count = items->size(), self](std::exception_ptr ep) {
        LOG("RX cache", warning) << "Caching aborted after processing "
          << count << " item(s) of type " << boost::typeindex::type_id<T>().pretty_name()
          << " due to exception: " << GetExceptionMessage(ep);
        self->finishRetrieving(nullptr); // Update our own state...
        self->processFollowers(); // ... then allow any followers to become the new primary subscriber...
        subscriber.on_error(ep); // ... before notifying the current primary subscriber, whose error handler may retry and create a new (primary) subscriber.
      },
      [subscriber, items, self]() {
        self->finishRetrieving(items); // Update our own state...
        self->mCreateSource = std::nullopt; // ... and discard the item retrieval lambda (including any resources such as captured variables)...
        subscriber.on_completed(); // ... then finish dealing with the primary subscriber...
        self->processFollowers(); // ... before emitting cached items to followers.
      });
  }
  
  void handleSubscriber(rxcpp::subscriber<T> subscriber) const {
    if (mItems != nullptr) {
      // We already have the items in our cache: simply emit them.
      this->emitItemsTo(subscriber);
    }
    else if (mRetrieving) {
      // A (primary) subscriber is already retrieving the items: enlist this one as a follower.
      mFollowers.push_back(subscriber);
    }
    else {
      // This will be the primary subscriber: let it retrieve items from the source.
      this->startRetrieving(subscriber);
    }
  }

  explicit WaitlessRxCache(const CreateSourceFunction& createSource)
    : mCreateSource(createSource) {
  }

public:
  rxcpp::observable<T> observe() const override {
    // Postpone emitting items until the caller subscribes (indicating that they want the items now)
    return CreateObservable<T>([self = SharedFrom(*this)](rxcpp::subscriber<T> subscriber) { self->handleSubscriber(subscriber); });
  }
};

}

template <typename CreateSource>
std::shared_ptr<RxCache<typename decltype(std::declval<CreateSource>()())::value_type>> CreateRxCache(const CreateSource& createSource) {
  using TCache = detail::WaitlessRxCache<typename decltype(std::declval<CreateSource>()())::value_type>;
  return std::shared_ptr<TCache>(new TCache([createSource]() {return createSource().as_dynamic(); }));
}

}
