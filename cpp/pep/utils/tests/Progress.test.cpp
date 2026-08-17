#include <gtest/gtest.h>

#include <pep/utils/Progress.hpp>

#include <stdexcept>

namespace {

TEST(ProgressTest, StartsUnfinished) {
  auto progress = pep::Progress::Create(3U);
  EXPECT_FALSE(progress->done());
  EXPECT_EQ(progress->describe(), "1/3");
}

TEST(ProgressTest, ZeroStepsIsImmediatelyDone) {
  auto progress = pep::Progress::Create(0U);
  EXPECT_TRUE(progress->done());
  EXPECT_EQ(progress->describe(), "done");
}

TEST(ProgressTest, DescribeIncludesStepName) {
  auto progress = pep::Progress::Create(3U);
  progress->advance("loading");
  EXPECT_EQ(progress->describe(), "1/3: loading");
  progress->advance("processing");
  EXPECT_EQ(progress->describe(), "2/3: processing");
}

TEST(ProgressTest, AdvanceStepsForwardByMultiple) {
  auto progress = pep::Progress::Create(5U);
  progress->advance(3U);
  EXPECT_EQ(progress->describe(), "3/5");
  progress->advance(2U);
  EXPECT_EQ(progress->describe(), "5/5");
  EXPECT_FALSE(progress->done()); // completion needs one more advance
}

TEST(ProgressTest, AdvanceToCompletionFinishes) {
  auto progress = pep::Progress::Create(2U);
  progress->advance("first");
  progress->advance("second");
  EXPECT_FALSE(progress->done());
  progress->advanceToCompletion();
  EXPECT_TRUE(progress->done());
  EXPECT_EQ(progress->describe(), "done");
}

TEST(ProgressTest, OnChangeCallbackFiresForEachAdvance) {
  auto progress = pep::Progress::Create(2U);
  unsigned notifications = 0U;
  auto subscription = progress->onChange.subscribe([&notifications](const pep::Progress&) { ++notifications; });

  progress->advance("a");            // notification 1
  progress->advance("b");            // notification 2
  progress->advanceToCompletion();   // notification 3
  EXPECT_EQ(notifications, 3U);
}

TEST(ProgressTest, OnChangeCallbackStopsAfterCancel) {
  auto progress = pep::Progress::Create(3U);
  unsigned notifications = 0U;
  auto subscription = progress->onChange.subscribe([&notifications](const pep::Progress&) { ++notifications; });

  progress->advance("a");
  subscription.cancel();
  progress->advance("b");
  EXPECT_EQ(notifications, 1U);
}

TEST(ProgressTest, GetStateReturnsSelf) {
  auto progress = pep::Progress::Create(2U);
  progress->advance();
  auto state = progress->getState();
  ASSERT_EQ(state.size(), 1U);
  EXPECT_EQ(state.top(), progress);
}

TEST(ProgressTest, ChildProgressAppearsInDescriptionAndState) {
  auto parent = pep::Progress::Create(2U);
  parent->advance("parent step");

  auto child = pep::Progress::Create(2U, parent->push());
  child->advance("child step");

  EXPECT_EQ(parent->describe(), "1/2: parent step - 1/2: child step");

  auto state = parent->getState();
  ASSERT_EQ(state.size(), 2U);
  EXPECT_EQ(state.top(), child); // deepest progress on top

  // Notifications from the child propagate to the parent.
  unsigned parentNotifications = 0U;
  auto subscription = parent->onChange.subscribe([&parentNotifications](const pep::Progress&) { ++parentNotifications; });
  child->advance("child step 2");
  EXPECT_EQ(parentNotifications, 1U);
}

TEST(ProgressTest, CompletedChildIsDetachedFromParent) {
  auto parent = pep::Progress::Create(2U);
  parent->advance("parent step");

  auto child = pep::Progress::Create(1U, parent->push());
  child->advance("child step");
  child->advanceToCompletion();
  EXPECT_TRUE(child->done());

  // Once the child is done it is no longer part of the parent's description or state.
  EXPECT_EQ(parent->describe(), "1/2: parent step");
  EXPECT_EQ(parent->getState().size(), 1U);
}

TEST(ProgressTest, PushOntoUnstartedSequenceThrows) {
  auto parent = pep::Progress::Create(2U);
  auto child = pep::Progress::Create(1U);
  auto push = parent->push();
  EXPECT_THROW(push(child), std::runtime_error);
}

TEST(ProgressTest, PushSecondChildThrows) {
  auto parent = pep::Progress::Create(2U);
  parent->advance("parent step");

  auto firstChild = pep::Progress::Create(1U, parent->push());
  auto secondChild = pep::Progress::Create(1U);
  auto push = parent->push();
  EXPECT_THROW(push(secondChild), std::runtime_error);
}

}
