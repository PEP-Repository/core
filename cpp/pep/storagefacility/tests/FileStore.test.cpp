#include <pep/storagefacility/FileStore.hpp>

#include <pep/storagefacility/Constants.hpp>
#include <pep/storagefacility/EntryPayload.hpp>
#include <pep/storagefacility/PersistedEntryProperties.hpp>
#include <pep/storagefacility/PrefixedStrings.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/async/tests/RxTestUtils.hpp>

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <iterator>
#include <numeric>
#include <sstream>
#include <gmock/gmock-more-matchers.h>

using pep::EntryContent;
using pep::FileStore;
using namespace std::ranges;

namespace {

constexpr pep::Timestamp operator""_unixMs(const unsigned long long ms) noexcept {
  return pep::Timestamp(std::chrono::milliseconds{ms});
}

struct Context {
  std::shared_ptr<boost::asio::io_context> io_context
    = std::make_shared<boost::asio::io_context>();

  std::filesystem::path path = std::filesystem::temp_directory_path() / "pep-sf-tests";
  std::string bucket = "myBucket";
  std::shared_ptr<FileStore> store;

  std::filesystem::path metapath() { return this->path / "meta"; }
  std::filesystem::path datapath() { return this->path / "data"; }
  std::filesystem::path bucketpath() {
    return this->datapath() / this->bucket;
  }

  ~Context() {
    std::filesystem::remove_all(this->path);
  }

  Context() {
    std::filesystem::create_directories(this->bucketpath());
    std::filesystem::create_directories(this->metapath());

    boost::property_tree::ptree localConf;
    localConf.put("DataDir", this->datapath().string());
    localConf.put("Bucket", this->bucket);

    boost::property_tree::ptree pageStoreConf;
    pageStoreConf.put_child("Local", localConf);

    this->store = FileStore::Create(
        this->metapath().string(), pep::Configuration::FromPtree(pageStoreConf), this->io_context,
        std::shared_ptr<prometheus::Registry>() // intentionally null
    );
  }

  template <typename T>
  std::shared_ptr<std::vector<T>> exhaust(rxcpp::observable<T> obs) {
    return pep::testutils::exhaust<T>(*(this->io_context), obs);
  }

  /// \brief Stores a (paged) entry consisting of the specified pages. The pages' data are not stored.
  void storePagedEntry(const pep::EntryName& name, std::vector<pep::PageId> pages, pep::Timestamp validFrom) const {
    auto change = this->store->modifyEntry(name, true);
    ASSERT_NE(change, nullptr);

    pep::PersistedEntryProperties properties;
    pep::SetPersistedEntryProperty(properties, "filesize", uint64_t{pages.size()});
    pep::SetPersistedEntryProperty(properties, "pagesize", uint64_t{1});
    change->setContent(std::make_unique<EntryContent>(
      EntryContent::Metadata(),
      EntryContent::PayloadData(
        {.polymorphicKey = pep::EncryptedKey(pep::CurvePoint::Random(), pep::CurvePoint::Random(), pep::CurvePoint::Random()), .blindingTimestamp = validFrom, .scheme = pep::EncryptionScheme::V3},
        std::make_shared<pep::PagedEntryPayload>(properties, std::move(pages)))));
    std::move(*change).commit(validFrom);
  }
};

/// Returns the lines that WritePrefixedStrings writes for the specified entries
std::vector<std::string> RenderLines(std::ranges::input_range auto&& entries) {
  std::ostringstream rendered;
  pep::WritePrefixedStrings(rendered, std::forward<decltype(entries)>(entries));
  std::vector<std::string> result;
  std::istringstream lines(std::move(rendered).str());
  for (std::string line; std::getline(lines, line);) {
    result.push_back(std::move(line));
  }
  return result;
}

/// Returns (full) "<participant>/<column>/<page>" paths for the store's (current) pages
std::vector<std::string> CollectPagePaths(const FileStore& store) {
  return RenderLines(store.pagePaths());
}

/// \brief Produces a participant name that the FileStore accepts, i.e. the text representation of a local pseudonym.
std::string RandomParticipant() {
  return pep::LocalPseudonym::Random().text();
}

std::vector<pep::PageId> PageIds(pep::PageId first, size_t count) {
  return views::iota(first, first + count) | to<std::vector>();
}

/// Returns the paths that the store should report for a cell's (latest) entry, as stored by Context::storePagedEntry
std::vector<std::string> ExpectedPagePaths(const std::string& participant, const std::string& column, const std::vector<pep::PageId>& pages) {
  return pages | views::transform([participant, column](pep::PageId page) {
    return std::format("{}/{}/{}.page", participant, column, page);
  }) | to<std::vector>();
}

std::vector<std::string> Sorted(std::vector<std::string> values) {
  sort(values);
  return values;
}


TEST(FileStore, Basic) {
  Context context;
  auto store = context.store;

  // Use typed NULL pointers to prevent ambiguity w.r.t. operator << selection in gtest. See e.g. https://github.com/google/googletest/issues/1616
  const std::shared_ptr<FileStore::Entry> nullentry;
  const std::shared_ptr<FileStore::EntryChange> nullchange;

  auto name = pep::EntryName(pep::LocalPseudonym::Random(), "test");
  pep::EncryptedKey polymorphicKey(pep::CurvePoint::Random(), pep::CurvePoint::Random(), pep::CurvePoint::Random());

  std::string page;
  for (size_t i = 0; page.size() < pep::InlinePageThreshold; i++)
    page += " " + std::to_string(i);

  auto change = store->modifyEntry(name, true);
  ASSERT_NE(nullchange, change);
  change->setContent(std::make_unique<EntryContent>(
    EntryContent::Metadata(),
    EntryContent::PayloadData(
      {.polymorphicKey = polymorphicKey, .blindingTimestamp = 1_unixMs, .scheme = pep::EncryptionScheme::V3},
      nullptr)));
  context.exhaust<std::string>(change->appendPage(std::make_shared<std::string>(page), page.size(), 0));
  std::move(*change).commit(1_unixMs);

  change = store->modifyEntry(name, false);
  ASSERT_NE(nullchange, change);
  EXPECT_EQ(1, change->content()->payload()->pageCount());

  auto duplicateChange = store->modifyEntry(name, false);
  std::move(*change).commit(3_unixMs);
  EXPECT_ANY_THROW(std::move(*duplicateChange).commit(3_unixMs));

  EXPECT_EQ(nullentry, store->lookup(name, 0_unixMs));

  auto test = store->lookup(name, 1_unixMs);
  ASSERT_NE(nullentry, test);
  EXPECT_EQ(1_unixMs, test->getValidFrom());
  EXPECT_EQ(1, test->content()->payload()->pageCount());

  test = store->lookup(name, 2_unixMs);
  ASSERT_NE(nullentry, test);
  EXPECT_EQ(1_unixMs, test->getValidFrom());
  EXPECT_EQ(1, test->content()->payload()->pageCount());

  test = store->lookup(name, 3_unixMs);
  ASSERT_NE(nullentry, test);
  EXPECT_EQ(3_unixMs, test->getValidFrom());
  EXPECT_EQ(1, test->content()->payload()->pageCount());
  {
    auto results = context.exhaust<std::shared_ptr<std::string>>(
        test->readPage(0));
    EXPECT_EQ(results->size(), 1);
    EXPECT_EQ(*((*results)[0]), page);
  }

  test = store->lookup(name, 4_unixMs);
  ASSERT_NE(nullentry, test);
  EXPECT_EQ(3_unixMs, test->getValidFrom());
  EXPECT_EQ(1, test->content()->payload()->pageCount());
  {
    auto results = context.exhaust<std::shared_ptr<std::string>>(
        test->readPage(0));
    EXPECT_EQ(results->size(), 1);
    EXPECT_EQ(*((*results)[0]), page);
  }

  auto pagePaths = CollectPagePaths(*store);
  ASSERT_THAT(pagePaths, testing::SizeIs(1)) << "Store should report the (single) page of the latest entry";
  EXPECT_TRUE(pagePaths.front().starts_with(std::format("{}{}", name, pep::EntryName::Delimiter)))
    << "Page path " << pagePaths.front() << " should be located under the entry's name";
  EXPECT_TRUE(std::filesystem::exists(context.bucketpath() / pagePaths.front()))
    << "Page path " << pagePaths.front() << " should refer to the stored page";
}

TEST(FileStore, PagePathsWithEmptyCell) {
  Context context;
  auto store = context.store;

  // Create a cell without committing an entry to it, e.g. like when a store is cancelled
  auto change = store->modifyEntry(pep::EntryName(pep::LocalPseudonym::Random(), "test"), true);
  ASSERT_NE(nullptr, change);
  change.reset();

  EXPECT_THAT(CollectPagePaths(*store), testing::IsEmpty());
}

TEST(FileStore, PagePathsWithEmptyEntry) {
  Context context;
  context.storePagedEntry(pep::EntryName(RandomParticipant(), "column"), {}, 1_unixMs);

  EXPECT_THAT(CollectPagePaths(*context.store), testing::IsEmpty());
}

TEST(FileStore, PagePathsOfMultipleCells) {
  Context context;
  std::vector<std::string> expected;
  for (pep::PageId participant = 0; participant < 3; ++participant) {
    auto participantName = RandomParticipant();
    for (pep::PageId column = 0; column < 2; ++column) {
      auto columnName = std::format("column{}", column);
      auto pages = PageIds(1000 * (participant * 2 + column), 3);
      context.storePagedEntry(pep::EntryName(participantName, columnName), pages, 1_unixMs);
      expected.append_range(ExpectedPagePaths(participantName, columnName, pages) | views::as_rvalue);
    }
  }

  EXPECT_EQ(Sorted(CollectPagePaths(*context.store)), Sorted(expected));
}

TEST(FileStore, PagePathsIncludeParticipantAddedDuringIteration) {
  Context context;
  const auto pages = PageIds(1, 3);
  // The added participant must sort after the first one, so that iteration (which follows the store's order) reaches it
  const auto first = RandomParticipant(), second = RandomParticipant();
  const auto& [participantA, participantB] = std::minmax(first, second);
  context.storePagedEntry(pep::EntryName(participantA, "column"), pages, 1_unixMs);

  // Page paths are produced lazily (e.g. as the Storage Facility sends messages), so the store may be modified while
  // we're iterating. Since we haven't reached it yet, a participant that's added in the meantime should be included.
  std::vector<pep::PrefixedStringsEntry> entries;
  for (auto&& entry : context.store->pagePaths()) {
    if (entries.empty()) {
      context.storePagedEntry(pep::EntryName(participantB, "column"), pages, 2_unixMs);
    }
    entries.push_back(std::move(entry));
  }

  auto expected = ExpectedPagePaths(participantA, "column", pages);
  expected.append_range(ExpectedPagePaths(participantB, "column", pages) | views::as_rvalue);
  EXPECT_EQ(RenderLines(entries), expected);
}

TEST(FileStore, PagePathsTolerateEntrySupersededDuringIteration) {
  Context context;
  const auto participant = RandomParticipant();
  const pep::EntryName name(participant, "column");
  const auto original = PageIds(1, 5);
  context.storePagedEntry(name, original, 1_unixMs);

  // Replace the cell's (latest) entry while we're still iterating over its page paths. We should keep reporting the
  // pages of the entry that we started with, i.e. iteration must not be affected by the original entry being discarded.
  std::vector<pep::PrefixedStringsEntry> entries;
  bool superseded = false;
  for (auto&& entry : context.store->pagePaths()) {
    auto isString = std::holds_alternative<pep::PrefixedString>(entry);
    entries.push_back(std::move(entry));
    if (isString && !superseded) {
      context.storePagedEntry(name, PageIds(1000000, 3), 2_unixMs);
      superseded = true;
    }
  }

  ASSERT_TRUE(superseded) << "Store should have produced page paths";
  EXPECT_EQ(RenderLines(entries), ExpectedPagePaths(participant, "column", original));
}

TEST(FileStore, PathTraversal) {
  Context context;
  auto store = context.store;

  auto dummyLp = pep::LocalPseudonym::Random();

  ASSERT_NO_THROW(store->modifyEntry({dummyLp, "test"}, true));

  EXPECT_THROW(store->modifyEntry({dummyLp, ".."}, true),
    std::invalid_argument);
  EXPECT_THROW(store->modifyEntry({dummyLp, "/test"}, true),
    std::runtime_error);
  EXPECT_THROW(store->modifyEntry({dummyLp, ""}, true),
    std::runtime_error);
}

}
