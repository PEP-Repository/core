#include <pep/utils/Bitpacking.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Raw.hpp>
#include <sstream>

#include <gtest/gtest.h>

namespace {

TEST(Raw, Integer) {
  for (uint64_t i = 0; i < std::numeric_limits<uint32_t>::max(); i = (i+1) * 10) {
    std::stringstream s;
    pep::WriteBinary(s, static_cast<uint32_t>(i));
    auto read = pep::ReadBinary(s, uint32_t{0});
    EXPECT_EQ(i, read);
  }
}

const std::string testString = "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Aenean commodo ligula eget dolor. Aenean massa. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Donec quam felis, ultricies nec, pellentesque eu, pretium quis, sem. Nulla consequat massa quis enim. Donec pede justo, fringilla vel, aliquet nec, vulputate eget, arcu. In enim justo, rhoncus ut, imperdiet a, venenatis vitae, justo. Nullam dictum felis eu pede mollis pretium. Integer tincidunt. Cras dapibus. Vivamus elementum semper nisi. Aenean vulputate eleifend tellus. Aenean leo ligula, porttitor eu, consequat vitae, eleifend ac, enim. Aliquam lorem ante, dapibus in, viverra quis, feugiat a, tellus. Phasellus viverra nulla ut metus varius laoreet. Quisque rutrum. Aenean imperdiet. Etiam ultricies nisi vel augue. Curabitur ullamcorper ultricies nisi. Nam eget dui."

"Etiam rhoncus. Maecenas tempus, tellus eget condimentum rhoncus, sem quam semper libero, sit amet adipiscing sem neque sed ipsum. Nam quam nunc, blandit vel, luctus pulvinar, hendrerit id, lorem. Maecenas nec odio et ante tincidunt tempus. Donec vitae sapien ut libero venenatis faucibus. Nullam quis ante. Etiam sit amet orci eget eros faucibus tincidunt. Duis leo. Sed fringilla mauris sit amet nibh. Donec sodales sagittis magna. Sed consequat, leo eget bibendum sodales, augue velit cursus nunc, quis gravida magna mi a libero. Fusce vulputate eleifend sapien. Vestibulum purus quam, scelerisque ut, mollis sed, nonummy id, metus. Nullam accumsan lorem in dui. Cras ultricies mi eu turpis hendrerit fringilla. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia Curae; In ac dui quis mi consectetuer lacinia."

"Nam pretium turpis et arcu. Duis arcu tortor, suscipit eget, imperdiet nec, imperdiet iaculis, ipsum. Sed aliquam ultrices mauris. Integer ante arcu, accumsan a, consectetuer eget, posuere ut, mauris. Praesent adipiscing. Phasellus ullamcorper ipsum rutrum nunc. Nunc nonummy metus. Vestibulum volutpat pretium libero. Cras id dui. Aenean ut eros et nisl sagittis vestibulum. Nullam nulla eros, ultricies sit amet, nonummy id, imperdiet feugiat, pede. Sed lectus. Donec mollis hendrerit risus. Phasellus nec sem in justo pellentesque facilisis. Etiam imperdiet imperdiet orci. Nunc nec neque. Phasellus leo dolor, tempus non, auctor et, hendrerit quis, nisi."

"Curabitur ligula sapien, tincidunt non, euismod vitae, posuere imperdiet, leo. Maecenas malesuada. Praesent congue erat at massa. Sed cursus turpis vitae tortor. Donec posuere vulputate arcu. Phasellus accumsan cursus velit. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia Curae; Sed aliquam, nisi quis porttitor congue, elit erat euismod orci, ac placerat dolor lectus quis orci. Phasellus consectetuer vestibulum elit. Aenean tellus metus, bibendum sed, posuere ac, mattis non, nunc. Vestibulum fringilla pede sit amet augue. In turpis. Pellentesque posuere. Praesent turpis. ";

TEST(Raw, String) {
  for (size_t i = 0; i < testString.size(); ++i) {
    std::string in = testString.substr(0, i);

    std::stringstream s;
    pep::WriteBinary(s, in);

    auto out = pep::ReadBinary(s, decltype(in)());
    EXPECT_EQ(in, out);
  }
}

TEST(Raw, Vector) {
  std::vector<uint32_t> in;
  in.reserve(1024);

  for (uint32_t i = 0; i < 1024; ++i) {
    in.clear();
    for (uint32_t j = 0; j < i; ++j) {
      in.push_back(j);
    }

    std::stringstream s;
    pep::WriteBinary(s, in);

    auto out = pep::ReadBinary(s, decltype(in)());
    EXPECT_EQ(in, out);
  }
}
TEST(Raw, Map) {
  std::map<uint32_t, uint32_t> in;

  for (uint32_t i = 0; i < 256; ++i) {
    in.clear();
    for (uint32_t j = 0; j < i; ++j) {
      in.emplace(j, j+1);
    }

    std::stringstream s;
    pep::WriteBinary(s, in);

    auto out = pep::ReadBinary(s, decltype(in)());
    EXPECT_EQ(in, out);
  }
}

TEST(Raw, PackedBE) {
  uint64_t value{};
  pep::RandomBytes(reinterpret_cast<uint8_t*>(&value), sizeof(uint64_t) / sizeof(uint8_t));
  auto packed = pep::PackUint64BE(value);

  std::stringstream stream;
  pep::WriteBinary(stream, value);

  EXPECT_EQ(packed, stream.str()) << "Big-endian packing and network-compatible binary writing don't produce the same string";
}

}
