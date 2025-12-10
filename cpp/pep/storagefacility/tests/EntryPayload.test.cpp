#include <memory>
#include <type_traits>

#include <pep/storagefacility/EntryPayload.hpp>
#include <pep/storagefacility/PersistedEntryProperties.hpp>

#include <gtest/gtest.h>

using namespace pep;

namespace {

/// Calls std::make_shared with the specific type T and than casts it to a std::shared_ptr of the base class
template <typename T, typename... Args>
requires std::is_base_of_v<EntryPayload, T>
std::shared_ptr<EntryPayload> MakeEntryPayload(Args&&... args) {
  return std::make_shared<T>(std::forward<Args>(args)...);
}

/// Record type of (only) entry properties that we use during tests
struct EntryProperties final {
  uint64_t filesize;
  uint64_t pagesize;
};

/// Convenience function to create PersistedEntryProperties objects for tests
PersistedEntryProperties AsPersistentEntryProperties(const EntryProperties vals) {
  PersistedEntryProperties persistedProps;
  SetPersistedEntryProperty(persistedProps, "filesize", vals.filesize);
  SetPersistedEntryProperty(persistedProps, "pagesize", vals.pagesize);
  return persistedProps;
}

} // namespace

TEST(EntryPayload, default_constructed_paged_payload_is_empty) {
  const auto default_constructed = std::make_shared<PagedEntryPayload>();
  EXPECT_FALSE(default_constructed->pageSize().has_value());
  EXPECT_EQ(default_constructed->pageCount(), 0);
  EXPECT_EQ(default_constructed->size(), 0);
}

TEST(EntryPayload, payloads_are_not_strictly_equal_if_types_are_different) {
  const auto pageIds = std::vector<PageId>{12, 13, 14};

  auto props = AsPersistentEntryProperties({.filesize = 11, .pagesize = 11});
  const auto paged = MakeEntryPayload<PagedEntryPayload>(props, std::vector<PageId>{12, 13, 14});
  const auto inlined = MakeEntryPayload<InlinedEntryPayload>("1 2 3 4 5 6", 11);

  EXPECT_FALSE(StrictlyEqual(*paged, *inlined));
  EXPECT_FALSE(StrictlyEqual(*inlined, *paged));
}

TEST(EntryPayload, inlined_payloads_are_strictly_equal_if_their_content_is_equal) {
  EXPECT_TRUE(StrictlyEqual(
      *MakeEntryPayload<InlinedEntryPayload>("1 2", 3),
      *MakeEntryPayload<InlinedEntryPayload>("1 2", 3)));
  EXPECT_TRUE(StrictlyEqual(
      *MakeEntryPayload<InlinedEntryPayload>("3 4 5", 5),
      *MakeEntryPayload<InlinedEntryPayload>("3 4 5", 5)));

  // edge_case: compare to self
  const auto specific_instance = MakeEntryPayload<InlinedEntryPayload>("ABCD", 4);
  EXPECT_TRUE(StrictlyEqual(*specific_instance, *specific_instance));
}

TEST(EntryPayload, inlined_payloads_are_not_strictly_equal_if_their_contents_differ) {
  EXPECT_FALSE(StrictlyEqual(
      *MakeEntryPayload<InlinedEntryPayload>("AAA", 3),
      *MakeEntryPayload<InlinedEntryPayload>("BBB", 3)));
  EXPECT_FALSE(
      StrictlyEqual(*MakeEntryPayload<InlinedEntryPayload>("CC", 2), *MakeEntryPayload<InlinedEntryPayload>("D", 1)));
}

TEST(EntryPayload, paged_payloads_are_strictly_equal_if_their_content_is_equal) {
  const auto propsA = EntryProperties{.filesize = 1024, .pagesize = 512};
  const auto propsB = EntryProperties{.filesize = 2048, .pagesize = 512};
  const auto pages_A = std::vector<PageId>{12};
  const auto pages_B = std::vector<PageId>{35, 6};

  auto props = AsPersistentEntryProperties(propsA);
  const auto paged_AA_0 = MakeEntryPayload<PagedEntryPayload>(props, pages_A); // constructor consumes props

  props = AsPersistentEntryProperties(propsA);
  const auto paged_AA_1 = MakeEntryPayload<PagedEntryPayload>(props, pages_A);

  props = AsPersistentEntryProperties(propsB);
  const auto paged_BB_0 = MakeEntryPayload<PagedEntryPayload>(props, pages_B);

  props = AsPersistentEntryProperties(propsB);
  const auto paged_BB_1 = MakeEntryPayload<PagedEntryPayload>(props, pages_B);

  EXPECT_TRUE(StrictlyEqual(*paged_AA_0, *paged_AA_1));
  EXPECT_TRUE(StrictlyEqual(*paged_BB_0, *paged_BB_1));

  // edge_case: compare to self
  EXPECT_TRUE(StrictlyEqual(*paged_AA_0, *paged_AA_0));

  // edge case: empty payloads are equal (regardless of how are constructed)
  props = AsPersistentEntryProperties({.filesize = 0, .pagesize = 0});
  const auto empty_0 = MakeEntryPayload<PagedEntryPayload>(props, std::vector<PageId>{});
  const auto empty_1 = std::make_shared<PagedEntryPayload>();
  EXPECT_TRUE(StrictlyEqual(*empty_0, *empty_1));
}

TEST(EntryPayload, paged_payloads_are_not_strictly_equal_if_their_contents_differ) {
  const auto propsA = EntryProperties{.filesize = 1024, .pagesize = 512};
  const auto propsB = EntryProperties{.filesize = 2048, .pagesize = 512};
  const auto pages_A = std::vector<PageId>{12};
  const auto pages_B = std::vector<PageId>{35, 6};

  auto props = AsPersistentEntryProperties(propsA);
  const auto paged_AA = MakeEntryPayload<PagedEntryPayload>(props, pages_A); // constructor consumes props

  props = AsPersistentEntryProperties(propsA);
  const auto paged_AB = MakeEntryPayload<PagedEntryPayload>(props, pages_B);

  props = AsPersistentEntryProperties(propsB);
  const auto paged_BA = MakeEntryPayload<PagedEntryPayload>(props, pages_A);

  props = AsPersistentEntryProperties(propsB);
  const auto paged_BB = MakeEntryPayload<PagedEntryPayload>(props, pages_B);

  EXPECT_FALSE(StrictlyEqual(*paged_AA, *paged_AB));
  EXPECT_FALSE(StrictlyEqual(*paged_AA, *paged_BA));
  EXPECT_FALSE(StrictlyEqual(*paged_AA, *paged_BB));
}
