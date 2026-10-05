#include <pep/utils/PropertySerializer.hpp>
#include <gtest/gtest.h>

namespace {
struct PathEncapsulator {
  std::filesystem::path path;
};
}

template <> struct pep::PropertySerializer<PathEncapsulator> : public PropertySerializerByValue<PathEncapsulator> {
  void write(boost::property_tree::ptree& destination, const PathEncapsulator& value) const override {
    SerializeProperties(destination, value.path);
  }

  PathEncapsulator read(const boost::property_tree::ptree& source, const pep::DeserializationContext& context) const override {
    return PathEncapsulator{
      .path = DeserializeProperties<std::filesystem::path>(source, context)
    };
  }
};


namespace {
struct PathEncapsulator2 {
  std::filesystem::path path;
};
}

template <> struct pep::PropertySerializer<PathEncapsulator2> : public PropertySerializerByReference<PathEncapsulator2> {
  void write(boost::property_tree::ptree& destination, const PathEncapsulator2& value) const override {
    SerializeProperties(destination, value.path);
  }

  void read(PathEncapsulator2& destination, const boost::property_tree::ptree& source, const pep::DeserializationContext& context) const override {
    destination.path = DeserializeProperties<std::filesystem::path>(source, context);
  }
};


namespace {

template <typename TEncapsulator>
void TestContextApplication() {
  using namespace pep;

  // A reference value
  const TEncapsulator expected{ .path = "relative.txt" };
  ASSERT_TRUE(expected.path.is_relative());

  // Store value in a ptree
  boost::property_tree::ptree ptree;
  SerializeProperties(ptree, expected);

  // Deserialization should produce the original value (or our test code is no good)
  DeserializationContext context;
  auto deserialized = DeserializeProperties<TEncapsulator>(ptree, context);
  EXPECT_EQ(expected.path, deserialized.path);

  // With context, deserialization should produce an absolute path
  const auto base = absolute(std::filesystem::current_path());
  context.add(TaggedBaseDirectory(base));
  deserialized = DeserializeProperties<TEncapsulator>(ptree, context);
  EXPECT_NE(expected.path, deserialized.path);
  EXPECT_TRUE(deserialized.path.is_absolute());
}


TEST(PropertySerializer, AppliesDeserializationContext) {
  TestContextApplication<PathEncapsulator>();
  TestContextApplication<PathEncapsulator2>();
}

}
