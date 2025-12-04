#include <memory>

#include <pep/storagefacility/EntryPayload.hpp>
#include <pep/storagefacility/PersistedEntryProperties.hpp>

#include <gtest/gtest.h>

using pep::EntryPayload;

TEST(EntryPayload, EntryPayloadEquality) {
    using BasePtr = std::shared_ptr<EntryPayload>;

    { // both InlinedEntryPayload objects
      const BasePtr lhs = std::make_shared<pep::InlinedEntryPayload>("1 2 3 4 5 6", 11);
      const BasePtr same = std::make_shared<pep::InlinedEntryPayload>("1 2 3 4 5 6", 11);
      const BasePtr different_data = std::make_shared<pep::InlinedEntryPayload>("1 2 ZZZ 5 6", 11);
      const BasePtr different_size = std::make_shared<pep::InlinedEntryPayload>("1 2 3 4 5 6", 7);

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
      const BasePtr lhs = std::make_shared<pep::PagedEntryPayload>(props, lhs_pages); // constructor consumes props
      props = lhs_props();
      const BasePtr same = std::make_shared<pep::PagedEntryPayload>(props, lhs_pages);
      props = diff_filesize_props();
      const BasePtr diff1 = std::make_shared<pep::PagedEntryPayload>(props, lhs_pages);
      props = diff_pagesize_props();
      const BasePtr diff2 = std::make_shared<pep::PagedEntryPayload>(props, lhs_pages);
      props = lhs_props();
      const BasePtr diff3 = std::make_shared<pep::PagedEntryPayload>(props, diff_pages);

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
      const BasePtr paged = std::make_shared<pep::PagedEntryPayload>(props, lhs_pages); // constructor consumes props
      const BasePtr inlined = std::make_shared<pep::InlinedEntryPayload>("1 2 3 4 5 6", 11);

      EXPECT_FALSE(StrictlyEqual(*paged, *inlined));
      EXPECT_FALSE(StrictlyEqual(*inlined, *paged));
    }
}

