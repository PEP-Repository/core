#include <pep/utils/LifeCycler.hpp>
#include <gtest/gtest.h>

class LifeCycleExposer : public pep::LifeCycler {
public:
  ~LifeCycleExposer() noexcept override { this->setStatus(Status::finalizing); }
  using LifeCycler::setStatus; // Make this protected method public so we can test
};

TEST(LifeCycler, SendsRequiredNotifications) {
  std::map<pep::LifeCycler::Status, size_t> received;

  pep::EventSubscription subscription; // Keep subscription alive until _after_ the life cycler is destroyed

  // Use a sub-scope so that our LifeCycleExposer instance gets destroyed
  {
    LifeCycleExposer cycler;
    subscription = cycler.onStatusChange.subscribe([&received](const pep::LifeCycler::StatusChange& change) {
      received[change.updated]++;
      });

    EXPECT_ANY_THROW(cycler.setStatus(pep::LifeCycler::Status::initialized)) << "Life cycler should require instances to become initializing before they are initialized";

    EXPECT_EQ(pep::LifeCycler::Status::uninitialized, cycler.setStatus(pep::LifeCycler::Status::initializing)) << "Life cycler should be able to start initializing";
    EXPECT_EQ(pep::LifeCycler::Status::initializing,  cycler.setStatus(pep::LifeCycler::Status::initializing)) << "Life cycler should allow assignment of current status (as a no-op)";
    EXPECT_EQ(pep::LifeCycler::Status::initializing,  cycler.setStatus(pep::LifeCycler::Status::initialized)) << "Life cycler should be able to become initialized";

    // Listener receives Status::reinitializing and Status::initializing notifications, but caller receives a single return value representing the Status::initialized state before both transitions
    EXPECT_EQ(pep::LifeCycler::Status::initialized,   cycler.setStatus(pep::LifeCycler::Status::initializing)) << "Life cycler should be able to become re-initializing";
  }

  EXPECT_FALSE(subscription.active()) << "Life cycler didn't cancel subscription";

  EXPECT_FALSE(received.contains(pep::LifeCycler::Status::uninitialized)) << "Expected no uninitialized notification";

  ASSERT_TRUE(received.contains(pep::LifeCycler::Status::reinitializing)) << "Expected reinitializing notification";
  EXPECT_EQ(1U, received[pep::LifeCycler::Status::reinitializing]) << "Expected a single reinitializing notification";

  ASSERT_TRUE(received.contains(pep::LifeCycler::Status::initializing)) << "Expected initializing notification";
  EXPECT_EQ(2U, received[pep::LifeCycler::Status::initializing]) << "Expected multiple initializing notifications";

  ASSERT_TRUE(received.contains(pep::LifeCycler::Status::initialized)) << "Expected initialized notification";
  EXPECT_EQ(1U, received[pep::LifeCycler::Status::initialized]) << "Expected a single initialized notification";

  ASSERT_TRUE(received.contains(pep::LifeCycler::Status::finalizing)) << "Expected finalizing notification";
  EXPECT_EQ(1U, received[pep::LifeCycler::Status::finalizing]) << "Expected a single finalizing notification";

  ASSERT_TRUE(received.contains(pep::LifeCycler::Status::finalized)) << "Expected finalized notification";
  EXPECT_EQ(1U, received[pep::LifeCycler::Status::finalized]) << "Expected a single finalized notification";

  EXPECT_EQ(received.size(), 5U) << "Life cycler sent unexpected notification(s)";
}
