#pragma once

#include <rxcpp/operators/rx-observe_on.hpp>
#include <rxcpp/operators/rx-subscribe_on.hpp>

#include <pep/async/CreateObservable.hpp>
#include <pep/async/IoContext_fwd.hpp>

namespace pep {

rxcpp::observe_on_one_worker observe_on_asio(boost::asio::io_context& io_context);

/*! \brief Post some work to the io_context, and return an observable that emits the result, and then completes
 *  \tparam T The type of the work. Should be [Callable](https://en.cppreference.com/w/cpp/named_req/Callable)
 *  \param io_context The io_context to run the work on
 *  \param func Callable that performs the work, e.g. lambda, std::function or std::bind
 *  \return observable that emits the result of the work, and then completes
 */
template<typename T>
rxcpp::observable<typename std::invoke_result<T>::type> run_on_asio(boost::asio::io_context& io_context, T func) {
  return CreateObservable<typename std::invoke_result<T>::type >(
      [func](rxcpp::subscriber<typename std::invoke_result<T>::type > subscriber){
    subscriber.on_next(std::invoke(func));
    subscriber.on_completed();
  }).subscribe_on(observe_on_asio(io_context));
}

}
