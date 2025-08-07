#include <pep/utils/Bitpacking.hpp>

#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

// When a c-string is converted to a std::string, the first null char is treated as the end of the string.
// We need to use std string_literals to get the desired behavior here.
using namespace std::literals::string_literals;

namespace {

TEST(Bitpacking, PackUint8) {
  EXPECT_EQ(pep::PackUint8(0), "\0"s);
  EXPECT_EQ(pep::PackUint8(1), "\x1"s);
  EXPECT_EQ(pep::PackUint8(std::numeric_limits<uint8_t>::max()), "\xFF"s);
}

TEST(Bitpacking, PackUint32BE) {
  EXPECT_EQ(pep::PackUint32BE(0), "\0\0\0\0"s);
  EXPECT_EQ(pep::PackUint32BE(1), "\0\0\0\x1"s);
  EXPECT_EQ(pep::PackUint32BE(std::numeric_limits<uint8_t>::max() << 8), "\0\0\xFF\0"s);
  EXPECT_EQ(pep::PackUint32BE(std::numeric_limits<uint32_t>::max()), "\xFF\xFF\xFF\xFF"s);
}

TEST(Bitpacking, PackUint64BE) {
  EXPECT_EQ(pep::PackUint64BE(0), "\0\0\0\0\0\0\0\0"s);
  EXPECT_EQ(pep::PackUint64BE(1), "\0\0\0\0\0\0\0\x1"s);
  EXPECT_EQ(pep::PackUint64BE(std::numeric_limits<uint8_t>::max() << 8), "\0\0\0\0\0\0\xFF\0"s);
  EXPECT_EQ(pep::PackUint64BE(std::numeric_limits<uint64_t>::max()), "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"s);
}

TEST(Bitpacking, UnpackUint32BE) {
  EXPECT_EQ(pep::UnpackUint32BE("\xF0\xE1\xD2\xC3"s), uint64_t{0xF0E1'D2C3});
  EXPECT_EQ(pep::UnpackUint32BE("\x11\x22\x33\x44"s), uint64_t{0x1122'3344});

  // leading null chars
  EXPECT_EQ(pep::UnpackUint32BE("\0\0\xAA\0"s), uint64_t{0x0000'AA00});

  // if there are less than 4 bytes in the string they are unpacked into the most significant bytes of the integer
  EXPECT_EQ(pep::UnpackUint32BE("\x42\x43"s), uint64_t{0x4243'0000});

  // limits
  EXPECT_EQ(pep::UnpackUint32BE(""s), 0);
  EXPECT_EQ(pep::UnpackUint32BE("\0"s), 0);
  EXPECT_EQ(pep::UnpackUint32BE("\xFF\xFF\xFF\xFF"s), std::numeric_limits<uint32_t>::max());

  // truncate additional chars
  EXPECT_EQ(pep::UnpackUint32BE("\xFF\xFF\xFF\xFF\xFF"s), std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(pep::UnpackUint32BE("\xFE\xDC\xBA\x98\x76"s), 0xFEDC'BA98);
}

TEST(Bitpacking, UnpackUint64BE) {
  EXPECT_EQ(pep::UnpackUint64BE("\xF0\xE1\xD2\xC3\xB4\xA5\x96\x75"s), uint64_t{0xF0E1'D2C3'B4A5'9675});
  EXPECT_EQ(pep::UnpackUint64BE("\x11\x22\x33\x44\x55\x66\x77\x88"s), uint64_t{0x1122'3344'5566'7788});

  // leading null chars
  EXPECT_EQ(pep::UnpackUint64BE("\0\0\0\xAA\0\0\0\0"s), uint64_t{0x0000'00AA'0000'0000});

  // if there are less than 8 bytes in the string they are unpacked into the most significant bytes of the integer
  EXPECT_EQ(pep::UnpackUint64BE("\x42\x43"s), uint64_t{0x4243'0000'0000'0000});

  // limits
  EXPECT_EQ(pep::UnpackUint64BE(""s), 0);
  EXPECT_EQ(pep::UnpackUint64BE("\0"s), 0);
  EXPECT_EQ(pep::UnpackUint64BE("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"s), std::numeric_limits<uint64_t>::max());

  // truncate additional chars
  EXPECT_EQ(pep::UnpackUint64BE("\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"s), std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(pep::UnpackUint64BE("\xFE\xDC\xBA\x98\x76\x54\x32\x10\xFF"s), 0xFEDC'BA98'7654'3210);
}

}
