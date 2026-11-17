#include <pep/messaging/MessageSequence.hpp>
#include <pep/serialization/Error.hpp>
#include <pep/serialization/ErrorSerializer.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Filesystem.hpp>
#include <pep/utils/Generator.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/TestError.hpp>

#include <rxcpp/operators/rx-concat_map.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <format>
#include <fstream>
#include <ios>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

TEST(MessageSequence, IStreamIsBatchedLazily) {
  /* So I want the stream to produce a couple of pages, ending with a page that isn't entirely full.
   * The naive and readable implementation would be
   *    const size_t bytes = pep::messaging::DefaultPageSize * 4.5;
   * but we'd have to static_cast the multiplication result to prevent conversion warnings:
   *    const auto bytes = static_cast<size_t>(pep::messaging::DefaultPageSize * 4.5);
   * Then Clang produces another conversion warning about the constant being promoted to double when it serves as
   * an operand to the multiplication. So we'd have to add another static_cast, yielding
   *    const auto bytes = static_cast<size_t>(static_cast<double>(pep::messaging::DefaultPageSize) * 4.5);
   * IMO that would be just as stupid and unreadable as this non-converting implementation:
   */
  const auto bytes
    = pep::messaging::DefaultPageSize * 4U  // Please give me four full pages...
    + pep::messaging::DefaultPageSize / 2U; // ... and another half-full page.
  // </rant>

  auto file = pep::filesystem::Temporary::MakeFile(pep::RandomString(bytes));
  auto stream = std::make_shared<std::ifstream>(file.path(), std::ios_base::in | std::ios_base::binary);

  auto received = std::make_shared<size_t>(0U);

  pep::messaging::IStreamToMessageBatches(stream)
    .concat_map([stream](pep::messaging::MessageSequence sequence) { // Equivalent to .concat() but our lambda gets invoked for every batch, allowing us to test stuff in it
        EXPECT_TRUE(stream->good());
        return sequence;
      })
    .subscribe(
      [bytes, received, stream](std::shared_ptr<std::string> single) {
        *received += single->size();
        auto done = (*received == bytes);
        auto exhausted = stream->eof();
        EXPECT_EQ(done, exhausted) << "Stream was " << (exhausted ? "" : "not ") << "exhausted"
          << " after receiving " << *received << " of " << bytes << " bytes"; // (A.o.) verifies that the stream is read "lazily", i.e. that it isn't exhausted yet when subscriber receives non-final parts
      },
      [](std::exception_ptr exception) {
        FAIL() << "Error reading istream as message batches: " << pep::GetExceptionMessage(exception);
      },
      []() {}
    );

  EXPECT_TRUE(stream->eof()) << "All data should have been read from stream";
  EXPECT_EQ(*received, bytes);
}

namespace {

pep::Generator<std::string> CountingMessages(size_t count, std::shared_ptr<size_t> produced) {
  for (size_t i = 0; i < count; ++i) {
    ++*produced;
    co_yield std::format("message {}", i);
  }
}

pep::Generator<std::string> FailingMessages() {
  co_yield "first";
  throw pep::TestError{};
}

struct ConsumedBatches {
  std::vector<std::shared_ptr<std::string>> messages;
  size_t batches{};
  size_t maxPendingBatches{}; // The maximum number of batches that the outer observable had produced before we processed them
  bool completed{};
  std::exception_ptr error;
};

/// \brief Consumes MessageBatches like the messaging Scheduler does: subscribing to (and exhausting) one batch at a time.
ConsumedBatches Consume(pep::messaging::MessageBatches batches) {
  ConsumedBatches result;
  std::vector<pep::messaging::MessageSequence> pending;
  batches.subscribe(
    [&pending](pep::messaging::MessageSequence batch) { pending.push_back(std::move(batch)); },
    [&result](std::exception_ptr error) { result.error = std::move(error); },
    [&result] { result.completed = true; });

  while (!pending.empty() && result.error == nullptr) {
    result.maxPendingBatches = std::max(result.maxPendingBatches, pending.size());
    auto batch = pending.front();
    pending.erase(pending.begin());
    ++result.batches;
    batch.subscribe(
      [&result](std::shared_ptr<std::string> message) { result.messages.push_back(std::move(message)); },
      [&result](std::exception_ptr error) { result.error = std::move(error); });
  }
  return result;
}

std::vector<std::string> Contents(const ConsumedBatches& consumed) {
  return consumed.messages
    | std::views::transform([](const std::shared_ptr<std::string>& message) { return *message; })
    | std::ranges::to<std::vector>();
}

}

TEST(MessageSequence, RangeIsBatchedLazily) {
  auto produced = std::make_shared<size_t>(0U);
  std::vector<pep::messaging::MessageSequence> pending;
  auto completed = false;
  pep::messaging::RangeToMessageBatches(CountingMessages(3U, produced)).subscribe(
    [&pending](pep::messaging::MessageSequence batch) { pending.push_back(std::move(batch)); },
    [](std::exception_ptr error) { FAIL() << pep::GetExceptionMessage(error); },
    [&completed] { completed = true; });

  ASSERT_EQ(pending.size(), 1U) << "A single batch should be produced upon subscription";
  EXPECT_EQ(*produced, 0U) << "No message should be produced before a batch is subscribed to";

  std::vector<std::string> received;
  size_t batches = 0U;
  while (!pending.empty()) {
    EXPECT_EQ(*produced, batches) << "A message should only be produced when its batch is subscribed to";
    auto batch = pending.front();
    pending.erase(pending.begin());
    batch.subscribe([&received](std::shared_ptr<std::string> message) { received.push_back(*message); });
    ++batches;
    EXPECT_LE(pending.size(), 1U) << "A followup batch should only be produced after the previous one has been exhausted";
  }

  EXPECT_TRUE(completed);
  EXPECT_EQ(received, (std::vector<std::string>{"message 0", "message 1", "message 2"}));
  EXPECT_EQ(batches, 4U) << "Expected a batch for each message, plus a final (empty) one";
}

TEST(MessageSequence, RangeElementsAreSerialized) {
  std::vector<pep::Error> errors{pep::Error("first"), pep::Error("second")};
  auto consumed = Consume(pep::messaging::RangeToMessageBatches(std::move(errors)));

  ASSERT_TRUE(consumed.completed);
  ASSERT_EQ(consumed.messages.size(), 2U);
  EXPECT_EQ(std::string(pep::Serialization::FromString<pep::Error>(*consumed.messages[0]).what()), "first");
  EXPECT_EQ(std::string(pep::Serialization::FromString<pep::Error>(*consumed.messages[1]).what()), "second");
  EXPECT_EQ(consumed.maxPendingBatches, 1U);
}

TEST(MessageSequence, RangeMessagesArePassedOn) {
  auto first = std::make_shared<std::string>("first");
  auto second = std::make_shared<std::string>("second");
  auto consumed = Consume(pep::messaging::RangeToMessageBatches(std::vector{first, second}));

  ASSERT_TRUE(consumed.completed);
  ASSERT_EQ(consumed.messages.size(), 2U);
  EXPECT_EQ(consumed.messages[0], first) << "Serialized messages should be passed on as-is";
  EXPECT_EQ(consumed.messages[1], second) << "Serialized messages should be passed on as-is";
}

TEST(MessageSequence, RangeErrorIsPropagated) {
  auto consumed = Consume(pep::messaging::RangeToMessageBatches(FailingMessages()));

  EXPECT_EQ(Contents(consumed), (std::vector<std::string>{"first"}));
  ASSERT_NE(consumed.error, nullptr);
  EXPECT_THROW(std::rethrow_exception(consumed.error), pep::TestError);
  EXPECT_FALSE(consumed.completed);
}

TEST(MessageSequence, EmptyRangeProducesNoMessages) {
  auto consumed = Consume(pep::messaging::RangeToMessageBatches(std::vector<std::string>{}));

  EXPECT_TRUE(consumed.completed);
  EXPECT_TRUE(consumed.messages.empty());
  EXPECT_EQ(consumed.error, nullptr);
}
