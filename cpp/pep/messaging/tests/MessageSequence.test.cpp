#include <pep/messaging/MessageSequence.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Filesystem.hpp>
#include <pep/utils/Random.hpp>
#include <gtest/gtest.h>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <fstream>

TEST(MessageSequence, IStreamIsBatchedLazily) {
  /* So I want the stream to produce a couple of pages, ending with a page that isn't entirely full.
   * The naive and readable implementation would be
   *    const size_t bytes = pep::messaging::DEFAULT_PAGE_SIZE * 4.5;
   * but we'd have to static_cast the multiplication result to prevent conversion warnings:
   *    const auto bytes = static_cast<size_t>(pep::messaging::DEFAULT_PAGE_SIZE * 4.5);
   * Then Clang produces another conversion warning about the constant being promoted to double when it serves as
   * an operand to the multiplication. So we'd have to add another static_cast, yielding
   *    const auto bytes = static_cast<size_t>(static_cast<double>(pep::messaging::DEFAULT_PAGE_SIZE) * 4.5);
   * IMO that would be just as stupid and unreadable as this non-converting implementation:
   */
  const auto bytes
    = pep::messaging::DEFAULT_PAGE_SIZE * 4U  // Please give me four full pages...
    + pep::messaging::DEFAULT_PAGE_SIZE / 2U; // ... and another half-full page.
  // </rant>

  auto file = pep::filesystem::Temporary::MakeFile(pep::RandomString(bytes));
  auto stream = std::make_shared<std::ifstream>(file.path().c_str(), std::ios_base::in | std::ios_base::binary);

  auto received = std::make_shared<size_t>(0U);

  pep::messaging::IStreamToMessageBatches(stream)
    .concat_map([stream](pep::messaging::MessageSequence sequence) {
        EXPECT_TRUE(stream->good());
        return sequence;
      })
    .subscribe(
      [bytes, received, stream](std::shared_ptr<std::string> single) {
        *received += single->size();
        auto done = (*received == bytes);
        auto more = stream->good();
        EXPECT_EQ(done, !more) << "Stream was " << (more ? "not " : "") << "exhausted"
          << " after receiving " << *received << " of " << bytes << " bytes";
      },
      [](std::exception_ptr exception) {
        FAIL() << "Error reading istream as message batches: " << pep::GetExceptionMessage(exception);
      },
      []() {}
    );

  EXPECT_TRUE(!stream->good()) << "All data should have been read from stream ";
  EXPECT_EQ(*received, bytes);
}
