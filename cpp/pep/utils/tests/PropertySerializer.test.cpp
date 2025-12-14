#include <pep/utils/PropertySerializer.hpp>
#include <gtest/gtest.h>

struct PathEncapsulator {
  std::filesystem::path mPath;
};

template <> struct pep::PropertySerializer<PathEncapsulator> : public PropertySerializerByValue<PathEncapsulator> {
  void write(boost::property_tree::ptree& destination, const PathEncapsulator& value) const override {
    SerializeProperties(destination, value.mPath);
  }

  PathEncapsulator read(const boost::property_tree::ptree& source, const pep::DeserializationContext& context) const override {
    return PathEncapsulator{
      .mPath = DeserializeProperties<std::filesystem::path>(source, context)
    };
  }
};


struct PathEncapsulator2 {
  std::filesystem::path mPath;
};

template <> struct pep::PropertySerializer<PathEncapsulator2> : public PropertySerializerByReference<PathEncapsulator2> {
  void write(boost::property_tree::ptree& destination, const PathEncapsulator2& value) const override {
    SerializeProperties(destination, value.mPath);
  }

  void read(PathEncapsulator2& destination, const boost::property_tree::ptree& source, const pep::DeserializationContext& context) const override {
    destination.mPath = DeserializeProperties<std::filesystem::path>(source, context);
  }
};


namespace {

template <typename TEncapsulator>
void TestContextApplication() {
  using namespace pep;

  // A reference value
  const TEncapsulator expected{ .mPath = "relative.txt" };
  ASSERT_TRUE(expected.mPath.is_relative());

  // Store value in a ptree
  boost::property_tree::ptree ptree;
  SerializeProperties(ptree, expected);

  // Deserialization should produce the original value (or our test code is no good)
  DeserializationContext context;
  auto deserialized = DeserializeProperties<TEncapsulator>(ptree, context);
  EXPECT_EQ(expected.mPath, deserialized.mPath);

  // With context, deserialization should produce an absolute path
  const auto base = absolute(std::filesystem::current_path());
  context.add(TaggedBaseDirectory(base));
  deserialized = DeserializeProperties<TEncapsulator>(ptree, context);
  EXPECT_NE(expected.mPath, deserialized.mPath);
  EXPECT_TRUE(deserialized.mPath.is_absolute());
}

}

TEST(PropertySerializer, AppliesDeserializationContext) {
  TestContextApplication<PathEncapsulator>();
  TestContextApplication<PathEncapsulator2>();
}
