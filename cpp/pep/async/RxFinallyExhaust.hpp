#pragma once

#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/async/OnAsio.hpp>

#include <rxcpp/operators/rx-finally.hpp>

namespace pep {

namespace detail {

// Implementor class for RxFinallyExhaust<> function (below).
template <typename TFinisher>
class RxFinallyExhaustOperator {
  static_assert(rxcpp::is_observable<TFinisher>::value, "TFinisher must be an observable type. Did you invoke RxFinallyExhaust with a parameterless function that returns an observable?");

public:
  using CreateFinisher = std::function<TFinisher()>;

private:
  CreateFinisher mCreate;
  rxcpp::observe_on_one_worker mSubscribeOn;

public:
  RxFinallyExhaustOperator(const CreateFinisher& create, rxcpp::observe_on_one_worker subscribeOn) : mCreate(create), mSubscribeOn(subscribeOn) {}

  template <typename TMainItem, typename MainSourceOperator>
  rxcpp::observable<TMainItem> operator()(rxcpp::observable<TMainItem, MainSourceOperator> items) const {
    return items // Return the (main observable's) items...
      .finally([create = mCreate, subscribeOn = mSubscribeOn]() { // ... and (create and) run the finalizer observable when the main one is unsubscribed
      create().subscribe_on(subscribeOn).subscribe(
        [](const auto&) {}, // ignore
        [](std::exception_ptr exception) { LOG("RX cleanup", error) << "Error exhausting finalizer observable: " << GetExceptionMessage(exception); },
        []() {} // ignore
      );
        });
  }
};

}

/*! \brief Exhausts a finisher observable after the primary observable has been exhausted.
 *         Propagates items from the primary observable, ignoring any items the finisher produces.
 * \tparam TCreateFinisher A callable type that accepts no parameters and returns the finisher observable.
 * \param subscribeOn The coordination on which the finisher observable should be subscribed.
 * \param create A callable that returns the finisher observable.
 *
 * \remark Intended for RX-based cleanup jobs, the finisher isn't created until the primary observable
 *         has been unsubscribed, allowing for e.g.
 *            auto items = obj->getItems().op(RxFinallyExhaust([obj]() { return obj->disconnect(); }, myCoordination));
 */
template <typename TCreateFinisher>
auto RxFinallyExhaust(rxcpp::observe_on_one_worker subscribeOn, TCreateFinisher&& create) {
  return detail::RxFinallyExhaustOperator<decltype(create())>(std::forward<TCreateFinisher>(create), subscribeOn);
}

/*! \brief Exhausts a finisher observable after the primary observable has been exhausted.
 *         Propagates items from the primary observable, ignoring any items the finisher produces.
 * \tparam TCreateFinisher A callable type that accepts no parameters and returns the finisher observable.
 * \param io_context The I/O context for the coordination on which the finisher observable should be subscribed.
 * \param create A callable that returns the finisher observable.
 *
 * \remark Intended for RX-based cleanup jobs, the finisher isn't created until the primary observable
 *         has been unsubscribed, allowing for e.g.
 *            auto items = obj->getItems().op(RxFinallyExhaust([obj]() { return obj->disconnect(); }, myIoContext));
 */
template <typename TCreateFinisher>
auto RxFinallyExhaust(boost::asio::io_context& io_context, TCreateFinisher&& create) {
  return RxFinallyExhaust(observe_on_asio(io_context), std::forward<TCreateFinisher>(create));
}

}
