#pragma once

#include <boost/noncopyable.hpp>
#include <cassert>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace pep {

namespace detail {

/* !
 * \brief Connects a notifier (see Event<> class) to a subscriber (see EventSubscription class).
 * \remark This is just a helper for the mentioned Event<> and EventSubscription classes. You don't need this class in your own code.
 */
class EventContract : boost::noncopyable {
public:
  virtual ~EventContract() noexcept = default;

  /* !
   * \brief Determines if the contract is active, i.e. if both notifier and subscriber are still maintaining their connection.
   * \return TRUE if both notifier and callback are still connected; FALSE otherwise.
   */
  virtual bool active() const noexcept = 0;

  /* !
   * \brief Disconnects the notifier from the subscriber (if they were still connected).
   */
  virtual void cancel() noexcept = 0;
};

}

/* !
 * \brief A subscription to a particular Event<>. Notifications will only be sent as long as the subscriber keeps the EventSubscription alive (and doesn't call its "cancel" method).
 * \remark Notifications will also stop if/when the associated Event<> is destroyed, after which the "active" method will return false.
 */
class EventSubscription : boost::noncopyable {
public:
  /* !
   * \brief Default constructor.
   * \remark Allows subscribers to have an (unassigned/empty) instance that will be assigned later.
   */
  EventSubscription() noexcept = default;

  /* !
   * \brief Move-constructor.
   * \remark E.g. allows EventSubscription to be emplaced into an std::map<>.
   */
  EventSubscription(EventSubscription&& other) noexcept;

  /* !
   * \brief Move-assignment operator.
   * \param other The instance from which to move-construct.
   * \remark Makes it (more) convenient for subscribers to keep hold of EventSubscription instancs,
   *         e.g. allowing them to (re-)assign to an initially default-constructed (empty) instance.
   */
  EventSubscription& operator=(EventSubscription&& other) noexcept;

  /* !
   * \brief Destructor, which will cancel the subscription if it was still active.
   */
  ~EventSubscription() noexcept;

  /* !
   * \brief Determines if the subscription is active (notifications will be received) or not.
   * \return TRUE if the callback will receive notifications; FALSE otherwise.
   * \remark A subscription (only) becomes inactive if/when the subscriber cancels it or the associated Event<> is destroyed.
   */
  bool active() noexcept;

  /* !
   * \brief Cancels the subscription (if it was still active), stopping the callback from receiving notifications.
   */
  void cancel() noexcept;

private:
  template <class TOwner, typename... TArgs>
  friend class Event;

  std::shared_ptr<detail::EventContract> mContract;
};


/* !
 * \brief A class that forwards notifications from an owner to (callbacks registered by) subscribers.
 * \tparam TOwner the class owning the event, i.e. which will send notifications.
 * \tparam TArgs the types of parameters that will be passed to event handlers.
 * \remark Many/most owning classes will encapsulate an Event<> as an instance (member) variable.
 *         Since we want to (be able to) subscribe to events even when the owning instance is const,
 *         this class's "subscribe" method is const as well (even though it updates state).
 *         This led to the entire interface being defined in terms of (possibly state-changing) const
 *         methods, allowing owners to simply expose their Event<> instances as public const member
 *         variables.
 */
template <class TOwner, typename... TArgs>
class Event final : boost::noncopyable {
  friend TOwner; // Only the owner class may invoke the "notify" method and control forwarding

public:
  /* !
   * \brief Constructor.
   * \remark A user-defined default constructor allows const Event<> instances to be default-initialized,
   *         in turn allowing owners to encapsulate a "const Event<> onSomething" without further code.
   *         See https://clang.llvm.org/compatibility.html#default_init_const
   */
  Event() noexcept(noexcept(std::vector<std::shared_ptr<Contract>>()) && noexcept(std::vector<EventSubscription>())) = default;

  /* !
   * \brief Destructor.
   * \remark Cancels all EventContracts of subscribed handlers, after which the associated EventSubscription::active() will return false.
   * \remark Also stops any forwarding that the Event<> may be performing.
   */
  ~Event() noexcept;

  /* !
   * \brief A callable type able to receive notifications from this event.
   */
  using Handler = std::function<void(TArgs...)>;
  
  /* !
   * \brief Subscribes a handler to this event, causing the handler to be invoked when the event is notified.
   * \param handler the function to be invoked when the event is notified.
   */
  EventSubscription subscribe(Handler handler) const;

private:
  /* !
   * \brief Notifies (invokes) all subscribed handlers.
   * \param args parameters to be passed to the handlers.
   */
  void notify(TArgs... args) const;

  /* !
   * \brief Causes this instance to forward notifications sent by another Event.
   * \param notifier The event sending the original notification.
   * \remark Utility helper: subscribes to the original Event so that the owner doesn't have to keep an
   *         EventSubscription alive just to set up forwarding (which is expected to be a common use case).
   *         Note that the EventSubscription is retained by this instance until "stopForwarding" is invoked.
   */
  template <typename T>
  void forward(const Event<T, TArgs...>& notifier) const;

  /* !
   * \brief Causes this instance to stop forwarding notifications for other Events, discarding associated
   *        EventSubscriptions.
   */
  void stopForwarding() const noexcept;

  class Contract;

  // See class documentation for rationale to have "mutable" member variables
  mutable std::vector<std::shared_ptr<Contract>> mContracts;
  mutable std::vector<EventSubscription> mForwardingSubscriptions;
};


template <class TOwner, typename... TArgs>
class Event<TOwner, TArgs...>::Contract : public detail::EventContract {
public:
  explicit Contract(Handler handler) : mHandler(std::move(handler)) {}

  bool active() const noexcept override { return mHandler.has_value(); }
  void cancel() noexcept override { mHandler.reset(); }
  bool notify(TArgs... args); // Returns TRUE if the contract was active and remained so; FALSE otherwise

private:
  std::optional<Handler> mHandler;
};


template <class TOwner, typename... TArgs>
EventSubscription Event<TOwner, TArgs...>::subscribe(Handler handler) const {
  auto contract = std::make_shared<Contract>(std::move(handler));
  mContracts.emplace_back(contract);
  EventSubscription result;
  result.mContract = std::move(contract);
  return result;
}

template <class TOwner, typename... TArgs>
bool Event<TOwner, TArgs...>::Contract::notify(TArgs... args) {
  if (!mHandler.has_value()) {
    return false;
  }

  // Use a copy of mHandler to prevent state corruption if the contract is cancelled during notification
  auto invokable = mHandler;
  (*invokable)(args...);

  return mHandler.has_value(); // Invocation of the handler may have cancel()led this contract
}

template <class TOwner, typename... TArgs>
Event<TOwner, TArgs...>::~Event() noexcept {
  for (auto& contract : mContracts) {
    contract->cancel();
  }
}

template <class TOwner, typename... TArgs>
void Event<TOwner, TArgs...>::notify(TArgs... args) const {
  // Apart from sending notifications, this method also performs housekeeping by discarding
  // contracts that have been cancelled (by the subscriber).
  auto clean = false;

  // We first (attempt to) notify all contracts that we're aware of...
  for (const auto& contract : mContracts) {
    if (!contract->notify(args...)) { // ... and if the contract has been cancelled...
      clean = true; // ... we'll want to discard it.
    }
  }

  // Then we discard cancelled contracts if we found any.
  if (clean) {
    std::erase_if(mContracts, [](const auto& contract) { return !contract->active(); });
  }
}

template <class TOwner, typename... TArgs> template <typename T>
void Event<TOwner, TArgs...>::forward(const Event<T, TArgs...>& notifier) const {
  // Ensure that we don't introduce an infinite loop by forwarding our own notifications
  // TODO: also prevent indirect circles: event1 -> event2 [ -> ... ] -> event1
  // TODO: also prevent duplicates: event1.forward(event2); event1.forward(event2);
  assert(this != static_cast<const void*>(&notifier)); // TODO: compare TOwner* instead of void* (i.e. only when TOwner and T are the same type, i.e. in specialized method)

  mForwardingSubscriptions.emplace_back(std::move(notifier.subscribe([this](TArgs... args) {
    this->notify(std::forward<TArgs>(args)...);
    })));
}

template <class TOwner, typename... TArgs>
void Event<TOwner, TArgs...>::stopForwarding() const noexcept {
  mForwardingSubscriptions.clear();
}

}
