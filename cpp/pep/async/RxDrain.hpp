#pragma once

#include <pep/async/CreateObservable.hpp>

namespace pep {

/* !
 * \brief Empties an std::queue<>, returning an observable that produces the queue's items.
 *
 * \tparam TItem The type of item stored in the queue.
 * \param queue: The queue whose items will be produced by the observable.
 * \return An observable that produces the queue's items, clearing the queue as items are being produced.
 *
 * \remark If you still need a container after converting it to an observable<>, use RxIterate<> instead (with an appropriate container type).
 */
template <typename TItem>
rxcpp::observable<TItem> RxDrain(std::shared_ptr<std::queue<TItem>> queue) {
  return CreateObservable<TItem>([queue](rxcpp::subscriber<TItem> subscriber) {
    while (subscriber.is_subscribed() && !queue->empty()) {
      subscriber.on_next(std::move(queue->front()));
      queue->pop();
    }

    // Clear queue in case we were unsubscribed before we finished iterating
    *queue = {};

    if (subscriber.is_subscribed()) {
      subscriber.on_completed();
    }
    });
}


}
