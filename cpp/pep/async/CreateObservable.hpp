#pragma once

#include <pep/utils/BuildFlavor.hpp>

#include <cassert>
#include <exception>

#include <rxcpp/rx-lite.hpp>

namespace pep {

// Like rxcpp::observable<>::create(), but catches exceptions of the
// callback and passes them to the subscribers' on_error.
template<class T, class OnSubscribe>
rxcpp::observable<T>
CreateObservable(OnSubscribe callback) {

#if BUILD_HAS_DEBUG_FLAVOR()
  // debug code to catch double subscriptions
  std::shared_ptr<bool> entered = std::make_shared<bool>(false);
#endif

  return rxcpp::observable<>::create<T>(  //IGNORE_OBSERVABLE_CREATE as this is the declaration of CreateObservable()
  [ callback{std::move(callback)}
#if BUILD_HAS_DEBUG_FLAVOR()
  , entered
#endif
  ] (rxcpp::subscriber<T> subscriber) {

#if BUILD_HAS_DEBUG_FLAVOR()
    assert(*entered==false /* ERROR: this observable is twice subscribed! */);
    // N.B. To pinpoint the cause of a double subscription, you can use the
    //      RxAssertNoMultipleSubscribers helper operator below.
    *entered = true;
#endif

    try {
      callback(subscriber);
    } catch (...) {
      subscriber.on_error(std::current_exception());
    }
  });
}

struct RxAssertNoMultipleSubscribers {
  template <typename T, typename Source>
  rxcpp::observable<T> operator()(rxcpp::observable<T, Source> obs) const {

#if BUILD_HAS_DEBUG_FLAVOR()
    return CreateObservable<T>([obs](rxcpp::subscriber<T> s){

      obs.subscribe(s);

    });
#else
    return obs;
#endif

  }
};

}
