#include <pep/storagefacility/FileStore.hpp>

#include <pep/storagefacility/Constants.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/async/tests/RxTestUtils.hpp>

#include <gtest/gtest.h>
#include <filesystem>

using pep::EntryContent;
using pep::FileStore;

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

    // create configuration
    std::stringstream ss;

    ss
      << "{\n"
      << "  \"Type\" : \"local\",\n"
      << "  \"DataDir\" : " << std::quoted(this->datapath().string()) << ",\n"
      << "  \"Bucket\" : " << std::quoted(this->bucket) << "\n"
      << "}\n";

      auto config = std::make_shared<pep::Configuration>(
          pep::Configuration::FromStream(ss));

    this->store = FileStore::Create(
        this->metapath().string(), config, this->io_context,
        std::shared_ptr<prometheus::Registry>() // intentionally null
    );
  }

  template <typename T>
  std::shared_ptr<std::vector<T>> exhaust(rxcpp::observable<T> obs) {
    return pep::testutils::exhaust<T>(*(this->io_context), obs);
  }
};


TEST(FileStore, Basic) {
  Context context;
  auto store = context.store;

  // Use typed NULL pointers to prevent ambiguity w.r.t. operator << selection in gtest. See e.g. https://github.com/google/googletest/issues/1616
  const std::shared_ptr<FileStore::Entry> nullentry;
  const std::shared_ptr<FileStore::EntryChange> nullchange;

  auto name = pep::EntryName(pep::LocalPseudonym::Random(), "test");
  pep::EncryptedKey polymorphicKey(pep::CurvePoint::Random(), pep::CurvePoint::Random(), pep::CurvePoint::Random());

  std::string page;
  for (size_t i = 0; page.size() < pep::INLINE_PAGE_THRESHOLD; i++)
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
}

}
