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
std::shared_ptr<EntryPayload> MakeEntryPayload(Args&&... args) { return std::make_shared<T>(std::forward<Args>(args)...); }

}

TEST(EntryPayload, EntryPayloadEquality) {
    { // both InlinedEntryPayload objects
      const auto lhs = MakeEntryPayload<InlinedEntryPayload>("1 2 3 4 5 6", 11);
      const auto same = MakeEntryPayload<InlinedEntryPayload>("1 2 3 4 5 6", 11);
      const auto different_data = MakeEntryPayload<InlinedEntryPayload>("1 2 ZZZ 5 6", 11);
      const auto different_size = MakeEntryPayload<InlinedEntryPayload>("1 2 3 4 5 6", 7);

      EXPECT_TRUE(StrictlyEqual(*lhs, *same));
      EXPECT_TRUE(StrictlyEqual(*lhs, *lhs)); // edge_case
      EXPECT_FALSE(StrictlyEqual(*lhs, *different_data));
      EXPECT_FALSE(StrictlyEqual(*lhs, *different_size));
    }
    { // both PagedEntryPayload objects
      const auto lhs_props = [] {
        PersistedEntryProperties props;
        SetPersistedEntryProperty(props, "filesize", uint64_t{1024});
        SetPersistedEntryProperty(props, "pagesize", uint64_t{512});
        return props;
      };
      const auto diff_filesize_props = [] {
        PersistedEntryProperties props;
        SetPersistedEntryProperty(props, "filesize", uint64_t{2048});
        SetPersistedEntryProperty(props, "pagesize", uint64_t{512});
        return props;
      };
      const auto diff_pagesize_props = [] {
        PersistedEntryProperties props;
        SetPersistedEntryProperty(props, "filesize", uint64_t{1024});
        SetPersistedEntryProperty(props, "pagesize", uint64_t{256});
        return props;
      };
      const auto lhs_pages = std::vector<PageId>{0xDEADBEEF};
      const auto diff_pages = std::vector<PageId>{0x12345678};

      auto props = lhs_props();
      const auto lhs = MakeEntryPayload<PagedEntryPayload>(props, lhs_pages); // constructor consumes props
      props = lhs_props();
      const auto same = MakeEntryPayload<PagedEntryPayload>(props, lhs_pages);
      props = diff_filesize_props();
      const auto diff1 = MakeEntryPayload<PagedEntryPayload>(props, lhs_pages);
      props = diff_pagesize_props();
      const auto diff2 = MakeEntryPayload<PagedEntryPayload>(props, lhs_pages);
      props = lhs_props();
      const auto diff3 = MakeEntryPayload<PagedEntryPayload>(props, diff_pages);

      EXPECT_TRUE(StrictlyEqual(*lhs, *same));
      EXPECT_TRUE(StrictlyEqual(*lhs, *lhs)); // edge_case
      EXPECT_FALSE(StrictlyEqual(*lhs, *diff1));
      EXPECT_FALSE(StrictlyEqual(*lhs, *diff2));
      EXPECT_FALSE(StrictlyEqual(*lhs, *diff3));
    }
    { // different object types
      const auto lhs_props = [] {
        PersistedEntryProperties props;
        SetPersistedEntryProperty(props, "filesize", uint64_t{11});
        SetPersistedEntryProperty(props, "pagesize", uint64_t{11});
        return props;
      };
      const auto lhs_pages = std::vector<PageId>{0xDEADBEEF};

      auto props = lhs_props();
      const auto paged = MakeEntryPayload<PagedEntryPayload>(props, lhs_pages); // constructor consumes props
      const auto inlined = MakeEntryPayload<InlinedEntryPayload>("1 2 3 4 5 6", 11);

      EXPECT_FALSE(StrictlyEqual(*paged, *inlined));
      EXPECT_FALSE(StrictlyEqual(*inlined, *paged));
    }
}

