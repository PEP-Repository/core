#include <pep/utils/Random.hpp>
#include <pep/utils/Stream.hpp>
#include <gtest/gtest.h>

TEST(Stream, Cropped) {
  constexpr std::streamsize max = 46;
  std::string buffer(max + 2, '\0');
  pep::RandomIStreamBuf random;

  // Test whether CroppingIStreamBuf actually crops
  pep::CroppingIStreamBuf limiting_buf(random, max);
  std::istream limiting_stream(&limiting_buf);
  limiting_stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  EXPECT_TRUE(limiting_stream.eof()) << "Stream should be at eof after limited read";
  EXPECT_EQ(max, limiting_stream.gcount()) << "Limited read should have extracted maximum number of characters";

  // Test whether CroppingIStreamBuf doesn't crop prematurely
  pep::CroppingIStreamBuf permissive_buf(random, static_cast<std::streamsize>(buffer.size() + 2U));
  std::istream permissive_stream(&permissive_buf);
  permissive_stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  EXPECT_FALSE(permissive_stream.eof()) << "Stream should not be at eof after permissive read";
  EXPECT_EQ(buffer.size(), static_cast<size_t>(permissive_stream.gcount())) << "Permissive read should have filled entire buffer";
  // Test whether CroppingIStreamBuf crops after successive read actions (i.e. retains state)
  permissive_stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  EXPECT_TRUE(permissive_stream.eof()) << "Permissive stream should be at eof after second read";
  EXPECT_EQ(static_cast<std::streamsize>(buffer.size()) - max, permissive_stream.gcount()) << "Permissive stream should have extracted remaining characters";
}
