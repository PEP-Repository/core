#include <gtest/gtest.h>

#include <pep/utils/Random.hpp>

#include <algorithm>
#include <vector>

namespace {

using Buffer = std::vector<std::byte>;

using RandomizeBuffer = std::function<void(Buffer&)>;

void TestZeroesFraction(const std::string& description, RandomizeBuffer randomize) {
  for (const auto size : { 512u, 500u, 500u /*repeated query*/, 1000u }) {
    std::vector<std::byte> buf(size);
    randomize(buf);

    auto numZeros = std::ranges::count(buf, std::byte{});
    EXPECT_LT(numZeros, buf.size() / 8)
      << description << " produced suspiciously many zeros in random buffer: " << numZeros << '/' << buf.size() << " bytes";
  }
}

TEST(Random, ZeroesFraction) {
  TestZeroesFraction("RandomBytes function", [](Buffer& buf) { pep::RandomBytes(buf); });

  pep::RandomIStreamBuf buf;
  std::istream stream(&buf);
  TestZeroesFraction("RandomIStreamBuf", [&stream](Buffer& buf) { stream.read(reinterpret_cast<char*>(buf.data()), buf.size()); });
}

}
