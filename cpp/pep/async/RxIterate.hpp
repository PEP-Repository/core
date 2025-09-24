#pragma once

#include <pep/async/CreateObservable.hpp>

namespace pep {

/* !
 * \brief Converts/adapts a container to an observable that produces the container's items.
 * 
 * \tparam TContainer The type of container to iterate over. Must satisfy (some of) the C++11 "Container" requirements: see https://en.cppreference.com/w/cpp/named_req/Container
 * \param container: The container whose items will be produced by the observable.
 * \return An observable that produces the container's items.
 * 
 * \remark As opposed to rxcpp::observable<>::iterate, this function creates no copies of the container. 
 * \remark If the container isn't needed anymore, consider using RxDrain<> instead.
 */
template <typename TContainer>
rxcpp::observable<typename TContainer::value_type> RxIterate(std::shared_ptr<TContainer> container) {
  return CreateObservable<typename TContainer::value_type>([container](rxcpp::subscriber<typename TContainer::value_type> subscriber) {
    for (auto i = container->cbegin(); subscriber.is_subscribed() && i != container->cend(); ++i) {
      subscriber.on_next(*i);
    }
    if (subscriber.is_subscribed()) {
      subscriber.on_completed();
    }
  });
}

}
