#include <pep/messaging/MessageSequence.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Filesystem.hpp>
#include <pep/utils/Random.hpp>
#include <gtest/gtest.h>
#include <fstream>

TEST(MessageSequence, IStreamIsBatchedLazily) {
  auto bytes = static_cast<size_t>(pep::messaging::DEFAULT_PAGE_SIZE * 4.5);
  auto file = pep::filesystem::Temporary::MakeFile(pep::RandomString(bytes));
  auto stream = std::make_shared<std::ifstream>(file.path().c_str(), std::ios_base::in | std::ios_base::binary);

  pep::messaging::IStreamToMessageBatches(stream)
    .subscribe(
      [stream](pep::messaging::MessageSequence) {},
      [](std::exception_ptr exception) {
        FAIL() << "Error reading istream as message batches: " << pep::GetExceptionMessage(exception);
      },
      []() {}
    );

  EXPECT_TRUE(stream->good()) << "No data should have been read from stream yet";
}
