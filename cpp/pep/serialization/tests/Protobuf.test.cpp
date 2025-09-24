#include <gtest/gtest.h>

#include <pep/serialization/ErrorSerializer.hpp>
#include <pep/serialization/Serialization.hpp>

using namespace pep;

namespace {

TEST(ProtobufTest, DeSerialization) {
  auto magic = MessageMagician<Error>::GetMagic();

  Error err("Something's wrong");
  std::string serialized = Serialization::ToString(err);
  ASSERT_EQ(magic, GetMessageMagic(serialized));

  auto deserialized = Serialization::FromString<Error>(serialized);
  EXPECT_EQ(std::string_view(err.what()), std::string_view(deserialized.what()))
      << "Error message could not be deserialized";
}

}
