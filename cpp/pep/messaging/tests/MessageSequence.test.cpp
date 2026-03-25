#include <pep/messaging/MessageSequence.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Filesystem.hpp>
#include <pep/utils/Random.hpp>
#include <gtest/gtest.h>
#include <rxcpp/operators/rx-concat_map.hpp>
#include <fstream>

TEST(MessageSequence, IStreamIsBatchedLazily) {
  auto bytes = static_cast<size_t>(pep::messaging::DEFAULT_PAGE_SIZE * 4.5);
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
        EXPECT_EQ(*received == bytes, !stream->good());
      },
      [](std::exception_ptr exception) {
        FAIL() << "Error reading istream as message batches: " << pep::GetExceptionMessage(exception);
      },
      []() {}
    );

  EXPECT_TRUE(!stream->good()) << "All data should have been read from stream ";
  EXPECT_EQ(*received, bytes);
}
