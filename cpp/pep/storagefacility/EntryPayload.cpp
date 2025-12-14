#include <pep/utils/Exceptions.hpp>
#include <pep/storagefacility/PageHash.hpp>
#include <pep/utils/Raw.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/storagefacility/EntryPayload.hpp>

#include <rxcpp/operators/rx-map.hpp>

namespace pep {

namespace {

const std::string FILE_SIZE_KEY = "filesize";
const std::string PAGE_SIZE_KEY = "pagesize";
const std::string INLINE_PAGE_KEY = "inline-page";

std::string GetPagePath(const EntryName& entry, XXH64_hash_t xxhash) {
  return entry.string() + EntryName::DELIMITER + std::to_string(xxhash) + ".page";
}

}

void EntryPayload::save(PersistedEntryProperties& properties, std::vector<PageId>& pages) const {
  SetPersistedEntryProperty(properties, FILE_SIZE_KEY, this->size());
  auto pagesize = this->pageSize();
  if (pagesize.has_value()) {
    SetPersistedEntryProperty(properties, PAGE_SIZE_KEY, *pagesize);
  }
}

void EntryPayload::Save(std::shared_ptr<EntryPayload> payload, PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  if (payload == nullptr) {
    SetPersistedEntryProperty(properties, FILE_SIZE_KEY, uint64_t{0});
  }
  else {
    payload->save(properties, pages);
    assert(properties.count(FILE_SIZE_KEY) != 0U);
    // Don't assert(properties.count(PAGE_SIZE_KEY) != 0U) since it doesn't hold for an empty PagedEntryPayload
  }
}

std::shared_ptr<EntryPayload> EntryPayload::Load(PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  std::shared_ptr<EntryPayload> result = InlinedEntryPayload::Load(properties, pages);
  if (result == nullptr) {
    result = PagedEntryPayload::Load(properties, pages);
    if (result == nullptr) {
      throw std::runtime_error("Can't load payload as inlined or paged");
    }
  }
  return result;
}

bool InlinedEntryPayload::allMemberVarsAreEqual(const EntryPayload& rhs) const {
  const auto& downcast = static_cast<const InlinedEntryPayload&>(rhs);
  return this->mContent == downcast.mContent
      && this->mPayloadSize == downcast.mPayloadSize;
}

messaging::MessageSequence InlinedEntryPayload::readPage(std::shared_ptr<PageStore> pageStore, const EntryName& name, size_t index) const {
  this->validatedPageIndex(index);
  return rxcpp::observable<>::just(MakeSharedCopy(mContent));
}

std::string InlinedEntryPayload::getEtag() const {
  auto xxhashstr = GetXxHashString(mContent);
  return ETag(mContent, xxhashstr);
}

void InlinedEntryPayload::save(PersistedEntryProperties& properties, std::vector<PageId>& pages) const {
  assert(pages.empty());

  SetPersistedEntryProperty(properties, INLINE_PAGE_KEY, mContent);
  EntryPayload::save(properties, pages);
}

std::shared_ptr<InlinedEntryPayload> InlinedEntryPayload::Load(PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  auto content = TryExtractPersistedEntryProperty<std::string>(properties, INLINE_PAGE_KEY);
  if (!content.has_value()) {
    return nullptr;
  }

  assert(pages.empty());
  auto size = ExtractFileSize(properties);
  [[maybe_unused]] auto pageSize = ExtractPageSize(properties);
  assert(size == pageSize || pageSize == 0U);

  return std::make_shared<InlinedEntryPayload>(std::move(*content), size);
}

bool PagedEntryPayload::allMemberVarsAreEqual(const EntryPayload& rhs) const {
  const auto& downcast = static_cast<const PagedEntryPayload&>(rhs);
  return this->mPages == downcast.mPages
      && this->mPayloadSize == downcast.mPayloadSize
      && this->mPageSize == downcast.mPageSize;
}

void PagedEntryPayload::save(PersistedEntryProperties& properties, std::vector<PageId>& pages) const {
  assert(pages.empty());

  pages = mPages;
  EntryPayload::save(properties, pages);
}

std::shared_ptr<PagedEntryPayload> PagedEntryPayload::Load(PersistedEntryProperties& properties, std::vector<PageId>& pages) {
  auto result = std::make_shared<PagedEntryPayload>(properties, std::move(pages));

  // We're expected to clear the "pages" vector, indicating to the caller that we've processed them.
  // Most likely our call to std::move has already left us with an empty vector, but according to
  // https://en.cppreference.com/w/cpp/utility/move :
  //    std::move is used to _indicate_ that an object t may be "moved from"
  // So since there's no guarantee, we invoke vector::clear explicitly.
  pages.clear();

  return result;
}

XXH64_hash_t EntryPayload::XxHash(const std::string& rawPage) {
  return XXH64(rawPage.data(), rawPage.length(), uint64_t{0});
}

std::string EntryPayload::XxHashToString(XXH64_hash_t xxhash) {
  std::ostringstream out;
  WriteBinary(out, uint64_t{xxhash});
  return std::move(out).str();
}

uint64_t EntryPayload::ExtractFileSize(PersistedEntryProperties& properties) {
  return ExtractPersistedEntryProperty<uint64_t>(properties, FILE_SIZE_KEY);
}

uint64_t EntryPayload::ExtractPageSize(PersistedEntryProperties& properties) {
  // Backward compatible: the "pagesize" property was added later, i.e. old entries don't have it.
  // See https://gitlab.pep.cs.ru.nl/pep/core/-/commit/8de467f1ddda2d0672e0a78fb670b0d9a70c976b#6aef02059815e8f42707322e6f46f5cfcb14ea65_609_568
  auto result = TryExtractPersistedEntryProperty<uint64_t>(properties, PAGE_SIZE_KEY);
  return result.value_or(uint64_t{0});
}

rxcpp::observable<std::string> PagedEntryPayload::appendPage(PageStore& pageStore, const EntryName& name, uint64_t pagenr, std::shared_ptr<std::string> rawPage, uint64_t payloadSize)
{
  if (this->pageCount() != pagenr)
    throw Error("Cannot append page: "
      "incorrect page sequence number.");

  // compute xxhash and prepare payload for S3/MD5
  auto xxhash = XxHash(*rawPage);
  auto xxhashstr = XxHashToString(xxhash);

  // Throw an exception when a duplicate hash is found
  if (std::find(mPages.begin(), mPages.end(), xxhash) != mPages.end()) {
    throw std::runtime_error("FileStore error, duplicate data hash found in Entry Change: " + name.string() + ", a hashing collision has (likely) occurred.");
  }

  this->mPages.push_back(xxhash);
  mPayloadSize += payloadSize;
  if (pagenr == 0) {
    mPageSize = payloadSize;
  }

  return pageStore.put(
    GetPagePath(name, xxhash),
    std::vector<std::shared_ptr<std::string>>{ rawPage,
    std::make_shared<std::string>(xxhashstr) });
}

size_t EntryPayload::validatedPageIndex(size_t index) const {
  if (index >= this->pageCount())
    throw std::invalid_argument("invalid page number");
  return index;
}

PagedEntryPayload::PagedEntryPayload(PersistedEntryProperties& properties, std::vector<PageId> pages)
  : mPages(std::move(pages)), mPayloadSize(ExtractFileSize(properties)), mPageSize(ExtractPageSize(properties)) {
}

std::optional<uint64_t> PagedEntryPayload::pageSize() const {
  if (mPageSize == 0U) {
    return std::nullopt;
  }
  return mPageSize;
}

messaging::MessageSequence PagedEntryPayload::readPage(std::shared_ptr<PageStore> pageStore, const EntryName& name, size_t index) const {
  index = this->validatedPageIndex(index);

  uint64_t expected_hash = mPages[index];
  std::string path = GetPagePath(name, expected_hash);

  return pageStore->get(path).map(

    [path, expected_hash](std::shared_ptr<std::string> data)
    -> std::shared_ptr<std::string> {

      size_t pagelength = data->length() - sizeof(uint64_t);

      std::string hash_str = data->substr(pagelength, sizeof(uint64_t));
      data->resize(pagelength);

      std::istringstream hash_ss(hash_str);

      uint64_t hash = ReadBinary(hash_ss, uint64_t{0});
      uint64_t computed_hash = EntryPayload::XxHash(*data);

      if (hash != computed_hash) {
        throw std::runtime_error("data corruption detected in page "
          + path + ": computed xxhash of page is "
          + std::to_string(computed_hash) + " instead of the stored value "
          + std::to_string(hash));
      }

      if (hash != expected_hash) {
        throw std::runtime_error("xxhash of page " + path + " is "
          + std::to_string(hash) + " instead of "
          + std::to_string(expected_hash) + "!");
      }

      return data;
    });
}

}
