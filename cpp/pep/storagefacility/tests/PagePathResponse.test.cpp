#include <pep/serialization/Serialization.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>

#include <gtest/gtest.h>

#include <sstream>

using namespace pep;

namespace {

PagePathResponse MakeResponse() {
  return PagePathResponse{
    .paths = {
      PushPrefix{.prefix = "participant1/"},
      PushPrefix{.prefix = "column1/"},
      PrefixedStrings{.strings = {"1.page", "2.page"}},
      PopPrefix{},
      PushPrefix{.prefix = "column2/"},
      PrefixedStrings{.strings = {"3.page"}},
      PopPrefix{},
      PopPrefix{},
      // Edge cases: default-valued (i.e. empty) oneof members must still be distinguishable after deserialization
      PushPrefix{.prefix = ""},
      PrefixedStrings{.strings = {}},
      PrefixedStrings{.strings = {"4.page"}},
      PrefixedString{.string = "5.page"},
      PrefixedString{.string = ""},
      PopPrefix{},
    },
  };
}

std::string Render(const PagePathResponse& response) {
  std::ostringstream out;
  WritePrefixedStrings(out, response.paths);
  return std::move(out).str();
}

TEST(PagePathResponse, SerializationRoundTrip) {
  auto original = MakeResponse();
  auto deserialized = Serialization::FromString<PagePathResponse>(Serialization::ToString(MakeResponse()));

  ASSERT_EQ(deserialized.paths.size(), original.paths.size());
  for (size_t i = 0; i < original.paths.size(); ++i) {
    EXPECT_EQ(deserialized.paths[i].index(), original.paths[i].index()) << "Entry " << i << " has the wrong type";
  }
  EXPECT_EQ(Render(original), "participant1/column1/1.page\nparticipant1/column1/2.page\nparticipant1/column2/3.page\n4.page\n5.page\n\n");
  EXPECT_EQ(Render(deserialized), Render(original));
}

TEST(PagePathResponse, EmptyRoundTrip) {
  auto deserialized = Serialization::FromString<PagePathResponse>(Serialization::ToString(PagePathResponse{}));
  EXPECT_TRUE(deserialized.paths.empty());
}

TEST(PagePathResponse, EntrySerializesWithMagic) {
  auto deserialized = Serialization::FromString<PrefixedStringsEntry>(Serialization::ToString(PrefixedStringsEntry{PushPrefix{.prefix = "x/"}}));
  ASSERT_TRUE(std::holds_alternative<PushPrefix>(deserialized));
  EXPECT_EQ(std::get<PushPrefix>(deserialized).prefix, "x/");
}

TEST(PagePathResponse, EntryWithoutValueIsRejected) {
  EXPECT_THROW(Serialization::FromProtocolBuffer(proto::PrefixedStringsEntry{}), std::invalid_argument);
}

}
