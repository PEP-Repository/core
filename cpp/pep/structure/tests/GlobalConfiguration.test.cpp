#include <gtest/gtest.h>

#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/serialization/Serialization.hpp>

#include <string>
#include <vector>

namespace {

pep::GlobalConfiguration MakeConfiguration(std::vector<pep::StudyContext> contexts) {
  return pep::GlobalConfiguration(
    {pep::PseudonymFormat("TEST", 3U)},
    std::move(contexts),
    {}, // short pseudonyms
    pep::UserPseudonymFormat("TESTUSER", 16U),
    {}, // additional stickers
    {}, // devices
    {}, // assessors
    {}, // column specifications
    {}  // short pseudonym errata
  );
}

pep::GlobalConfiguration RoundTrip(const pep::GlobalConfiguration& config) {
  return pep::Serialization::FromString<pep::GlobalConfiguration>(pep::Serialization::ToString(config));
}

TEST(GlobalConfigurationSerializersTest, RoundTripsWithoutConfiguredStudyContexts) {
  auto deserialized = RoundTrip(MakeConfiguration({}));
  const auto& items = deserialized.getStudyContexts().getItems();
  ASSERT_EQ(items.size(), 1U);
  EXPECT_TRUE(items.front().isDefault());
  EXPECT_EQ(items.front().getId(), "");
}

TEST(GlobalConfigurationSerializersTest, RoundTripWithConfiguredStudyContexts) {
  auto deserialized = RoundTrip(MakeConfiguration({pep::StudyContext("first"), pep::StudyContext("second")}));
  const auto& items = deserialized.getStudyContexts().getItems();
  ASSERT_EQ(items.size(), 2U);
  EXPECT_EQ(items[0].getId(), "first");
  EXPECT_TRUE(items[0].isDefault());
  EXPECT_EQ(items[1].getId(), "second");
  EXPECT_FALSE(items[1].isDefault());
}

}
