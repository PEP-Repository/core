#include <pep/storagefacility/PagePathResponseChunker.hpp>

#include <pep/serialization/Serialization.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>
#include <pep/utils/Generator.hpp>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <format>
#include <ranges>
#include <sstream>
#include <gmock/gmock-more-matchers.h>

using namespace pep;

namespace {

Generator<PrefixedStringsEntry> Generate(std::vector<PrefixedStringsEntry> entries) {
  for (auto& entry : entries) {
    co_yield std::move(entry);
  }
}

/// \brief Produces entries for a hierarchy of participants, columns and pages, (like FileStore::pagePaths) with each page
///        as a separate PrefixedString.
std::vector<PrefixedStringsEntry> MakeEntries() {
  std::vector<PrefixedStringsEntry> result;
  for (size_t p = 0; p < 5; ++p) {
    result.emplace_back(PushPrefix{.prefix = std::format("participant{}/", p)});
    for (size_t c = 0; c < 4; ++c) {
      result.emplace_back(PushPrefix{.prefix = std::format("column{}/", c)});
      if (c != 2U) { // Leave some columns empty
        for (size_t page = 0; page < 7 * (p + 1); ++page) {
          result.emplace_back(PrefixedString{.string = std::format("{}.page", 1000000 + page)});
        }
      }
      result.emplace_back(PopPrefix{});
    }
    result.emplace_back(PopPrefix{});
  }
  result.emplace_back(PushPrefix{.prefix = "participant-without-columns/"});
  result.emplace_back(PopPrefix{});
  return result;
}

std::vector<PagePathResponse> Chunk(std::vector<PrefixedStringsEntry> entries, size_t capacity) {
  return ChunkPagePaths(Generate(std::move(entries)), capacity) | std::ranges::to<std::vector>();
}

std::string Render(const std::vector<PrefixedStringsEntry>& entries) {
  std::ostringstream out;
  WritePrefixedStrings(out, entries);
  return std::move(out).str();
}

TEST(PagePathResponseChunker, ChunksAreSelfContainedAndFitCapacity) {
  const auto expected = Render(MakeEntries());

  for (size_t capacity : {150U, 200U, 1000U, 1024U * 1024U}) {
    auto chunks = Chunk(MakeEntries(), capacity);
    ASSERT_FALSE(chunks.empty());
    if (capacity < expected.size()) {
      EXPECT_GT(chunks.size(), 1U) << "Data should have been split into multiple chunks for capacity " << capacity;
    }
    else {
      EXPECT_EQ(chunks.size(), 1U) << "Data should have fit into a single chunk for capacity " << capacity;
    }

    std::string output;
    for (const auto& chunk : chunks) {
      EXPECT_LE(Serialization::ToString(chunk, false).size(), capacity) << "Chunk exceeds capacity " << capacity;

      unsigned depth = 0;
      for (const auto& entry : chunk.paths) {
        EXPECT_FALSE(std::holds_alternative<PrefixedString>(entry)) << "Single strings should have been combined into PrefixedStrings";
        if (std::holds_alternative<PushPrefix>(entry)) {
          ++depth;
        }
        else if (std::holds_alternative<PopPrefix>(entry)) {
          ASSERT_GT(depth, 0) << "Chunk pops more prefixes than it pushed";
          --depth;
        }
      }
      EXPECT_EQ(depth, 0) << "Chunk should pop all prefixes that it pushes";

      output += Render(chunk.paths); // Rendering chunks individually requires them to be self-contained
    }
    EXPECT_EQ(output, expected) << "Chunks for capacity " << capacity << " should (together) contain all prefixed strings";
  }
}

TEST(PagePathResponseChunker, CombinesStringsUnderSamePrefixes) {
  auto chunks = Chunk({
    PushPrefix{.prefix = "a/"},
    PrefixedString{.string = "1"},
    PrefixedStrings{.strings = {"2", "3"}},
    PrefixedString{.string = "4"},
    PushPrefix{.prefix = "b/"},
    PrefixedString{.string = "5"},
    PopPrefix{},
    PrefixedString{.string = "6"},
    PopPrefix{},
    }, 1000U);

  ASSERT_EQ(chunks.size(), 1U);
  const auto& paths = chunks.front().paths;
  ASSERT_EQ(paths.size(), 7U);
  EXPECT_EQ(std::get<PushPrefix>(paths[0]).prefix, "a/");
  EXPECT_EQ(std::get<PrefixedStrings>(paths[1]).strings, (std::vector<std::string>{"1", "2", "3", "4"}));
  EXPECT_EQ(std::get<PushPrefix>(paths[2]).prefix, "b/");
  EXPECT_EQ(std::get<PrefixedStrings>(paths[3]).strings, (std::vector<std::string>{"5"}));
  EXPECT_TRUE(std::holds_alternative<PopPrefix>(paths[4]));
  EXPECT_EQ(std::get<PrefixedStrings>(paths[5]).strings, (std::vector<std::string>{"6"}));
  EXPECT_TRUE(std::holds_alternative<PopPrefix>(paths[6]));
}

TEST(PagePathResponseChunker, EmptyHierarchyProducesNoChunks) {
  EXPECT_THAT(Chunk({
      PushPrefix{.prefix = "participant/"}, PushPrefix{.prefix = "column/"},
      PopPrefix{}, PopPrefix{}},
    1000U),
    testing::IsEmpty());
  EXPECT_THAT(Chunk({}, 1000U), testing::IsEmpty());
}

TEST(PagePathResponseChunker, RejectsStringsThatDontFit) {
  EXPECT_THROW(Chunk({PushPrefix{.prefix = "participant/"}, PrefixedString{.string = std::string(50, 'x')}, PopPrefix{}}, 50U), std::runtime_error);
}

TEST(PagePathResponseChunker, RejectsUnbalancedInput) {
  EXPECT_THROW(Chunk({PopPrefix{}}, 1000U), std::logic_error);
  EXPECT_THROW(Chunk({PushPrefix{.prefix = "participant/"}}, 1000U), std::logic_error);
}

}
