#include <pep/utils/Event.hpp>
#include <gtest/gtest.h>

namespace {

class Notifier {
public:
  using Handler = pep::Event<Notifier>::Handler;
  const pep::Event<Notifier> onDoing;
  void doIt() const { onDoing.notify(); }
};

TEST(Event, Basics) {
  Notifier notifier;

  size_t notifications = 0U;
  auto registerNotification = [&notifications]() {++notifications; };

  // Discard (don't save) the subscription: notification should not be received
  notifier.onDoing.subscribe(registerNotification);
  notifier.doIt();
  ASSERT_EQ(notifications, 0U) << "Event sent notification despite subscription having been discarded";

  // Save the subscription: notification should be received
  auto subscription = notifier.onDoing.subscribe(registerNotification);
  ASSERT_TRUE(subscription.active()) << "Fresh subscription registers as inactive";
  notifier.doIt();
  ASSERT_EQ(notifications, 1U) << "Event notification was not received";

  {
    // Subscribe a second time
    auto subscription2 = notifier.onDoing.subscribe(registerNotification);
    ASSERT_TRUE(subscription2.active()) << "Second subscription registers as inactive";

    // Since we subscribed twice, we should receive two notifications
    notifier.doIt();
    ASSERT_EQ(notifications, 3U) << "Did not receive multiple event notifications";

    // Second subscription goes out of scope and should be cancelled
  }
  ASSERT_TRUE(subscription.active()) << "Subscriptions should be unaffected by other subscriptions being cancelled";
  notifier.doIt();
  ASSERT_EQ(notifications, 4U) << "Only the single remaining subscription should be notified";

  // Cancel the subscription: notification should not be received
  subscription.cancel();
  ASSERT_FALSE(subscription.active()) << "Subscription registers as active after cancellation";
  notifier.doIt();
  ASSERT_EQ(notifications, 4U) << "Event notification was received after cancellation";
}


// Segfaults if Event<>::Contract::notify invokes the handler directly from (a reference to) its mHandler.
TEST(Event, UnsubscribeDuringNotification) {
  class Forwarder {
  private:
    pep::EventSubscription mSubscription;
  public:
    explicit Forwarder(const Notifier& notifier, const Notifier::Handler& handler) {  // If Event<>::Contract::notify invokes its mHandler directly...
      mSubscription = notifier.onDoing.subscribe(                                     // ... then (the Event<>::Contract's copy of) the lambda...
        [this, handler]() {                                                           // ... that we define here...
          mSubscription.cancel();                                                     // ... is destroyed here, corrupting the captured "handler" variable...
          handler();                                                                  // ... causing a segfault here.
        });
    }
  };


  Notifier notifier;
  auto notified = false;
  Forwarder forwarder(notifier, [&]() { notified = true; });
  notifier.doIt();
  EXPECT_TRUE(notified) << "Notification wasn't received";
}


// Reads outside (formally) accessible memory if Event<>::notify iterates directly over its mContracts
TEST(Event, NotificationReentrancy) {
  Notifier notifier;
  auto subscription = std::make_shared<pep::EventSubscription>();
  *subscription = notifier.onDoing.subscribe([&notifier, subscription]() {
    subscription->cancel();  // Makes the contract inactive...
    notifier.doIt();         // ... causing it to be removed from the Event<>'s mContracts here...
    });

  notifier.doIt();           // ... causing (memcheck to detect) an "Invalid read" during this notification if Event<>::notify iterates directly over its mContracts
}


// Segfaults if Event<>::notify updates encapsulated state (after it has been destroyed).
TEST(Event, DestructionDuringNotification) {
  auto notifier = std::make_shared<Notifier>();
  auto subscription = notifier->onDoing.subscribe([&notifier]() {
    notifier.reset();
    });
  notifier->doIt();
}

}
