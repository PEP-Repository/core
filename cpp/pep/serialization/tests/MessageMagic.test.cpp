#include <gtest/gtest.h>

#include <pep/serialization/MessageMagic.hpp>
#include <pep/serialization/Error.hpp>
#include <pep/serialization/tests/VerifyBackwardCompatible.hpp>
#include <pep/utils/Timestamp.hpp>

namespace {

TEST(MessageMagic, Known) {
  std::string name = "Error"; // The only message type available in the Serialization lib
  auto normalized = pep::GetNormalizedTypeName<pep::Error>();
  ASSERT_EQ(name, normalized) << "Type name not normalized in a backward compatible way";

  auto magic = pep::CalculateMessageMagic(normalized);
  auto description = pep::DescribeMessageMagic(magic);
  ASSERT_EQ(description, name) << "MessageMagic for type " << name << " wasn't registered";
}

TEST(MessageMagic, TimestampHasBackwardCompatibleSerialization) {
  pep::VerifyBackwardCompatibleSerialization<pep::Timestamp>("Timestamp", 2154686979 /*taken from before commit 21fbce07*/);
}

}
