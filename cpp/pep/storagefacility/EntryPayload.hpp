#pragma once

#include <pep/storagefacility/EntryName.hpp>
#include <pep/storagefacility/PersistedEntryProperties.hpp>
#include <pep/storagefacility/PageStore.hpp>
#include <pep/messaging/MessageSequence.hpp>

#include <typeinfo>
#include <xxhash.h>

namespace pep {

using PageId = uint64_t;

/*!
 * \brief Base class for entry payloads: sequences of pages containing (encrypted) cell data.
 */
class EntryPayload {
protected:
  /// Equivalent to typeid(*this), but without requiring rtti
  virtual const std::type_info& type() const noexcept= 0;
  /// Polymorhic equality check that may only be called when both objects are of the exact same type
  virtual bool equals(const EntryPayload&) const = 0;

  size_t validatedPageIndex(size_t index) const;

  static XXH64_hash_t XxHash(const std::string& rawPage);
  static std::string XxHashToString(XXH64_hash_t xxhash);
  static std::string GetXxHashString(const std::string& rawPage) { return XxHashToString(XxHash(rawPage)); }

  static uint64_t ExtractFileSize(PersistedEntryProperties& properties);
  static uint64_t ExtractPageSize(PersistedEntryProperties& properties);

  virtual void save(PersistedEntryProperties& properties, std::vector<PageId>& pages) const;

  EntryPayload() = default;
  EntryPayload(const EntryPayload &) = default;
  EntryPayload& operator=(const EntryPayload &) = default;

public:
  /// True if both objects are of the exact same type AND they hold equivalent data
  bool operator== (const EntryPayload& rhs) const { return this->type() == rhs.type() && this->equals(rhs); }
  bool operator!= (const EntryPayload& rhs) const { return !(*this == rhs); }

  virtual ~EntryPayload() noexcept = default;

  virtual std::shared_ptr<EntryPayload> clone() const = 0;

  virtual size_t pageCount() const noexcept = 0;
  virtual uint64_t size() const noexcept = 0;
  virtual std::optional<uint64_t> pageSize() const = 0;
  virtual messaging::MessageSequence readPage(std::shared_ptr<PageStore> pageStore, const EntryName& name, size_t index) const = 0;

  static void Save(std::shared_ptr<EntryPayload> payload, PersistedEntryProperties& properties, std::vector<PageId>& pages);
  static std::shared_ptr<EntryPayload> Load(PersistedEntryProperties& properties, std::vector<PageId>& pages);
};

/*!
 * \brief An entry payload consisting of a single small page stored on the FileStore (i.e. without using the PageStore).
 * \remark The size limit is determined by the INLINE_PAGE_THRESHOLD constant.
 */
class InlinedEntryPayload : public EntryPayload {
private:
  std::string mContent;
  uint64_t mPayloadSize;

protected:
  const std::type_info& type() const noexcept override { return typeid(InlinedEntryPayload); };
  bool equals(const EntryPayload& rhs) const override {
    const auto& typed_rhs = static_cast<const InlinedEntryPayload&>(rhs);
    return this->mContent == typed_rhs.mContent && this->mPayloadSize == typed_rhs.mPayloadSize;
  }

  void save(PersistedEntryProperties& properties, std::vector<PageId>& pages) const override;

public:
  InlinedEntryPayload(std::string content, uint64_t payloadSize) : mContent(std::move(content)), mPayloadSize(payloadSize) {}

  std::shared_ptr<EntryPayload> clone() const override { return std::make_shared<InlinedEntryPayload>(*this); }

  size_t pageCount() const noexcept override { return 1U; }
  uint64_t size() const noexcept override { return mPayloadSize; }
  std::optional<uint64_t> pageSize() const override { return this->size(); }

  messaging::MessageSequence readPage(std::shared_ptr<PageStore> pageStore, const EntryName& name, size_t index) const override;
  std::string getEtag() const;

  static std::shared_ptr<InlinedEntryPayload> Load(PersistedEntryProperties& properties, std::vector<PageId>& pages);
};

/*!
 * \brief An entry payload whose pages are stored in a PageStore.
 */
class PagedEntryPayload : public EntryPayload {
private:
  std::vector<PageId> mPages;
  uint64_t mPayloadSize = 0;
  uint64_t mPageSize = 0; // Zero for old entries that didn't store the property

protected:
  const std::type_info& type() const noexcept override { return typeid(PagedEntryPayload); };
  bool equals(const EntryPayload& rhs) const override {
    const auto& typed_rhs = static_cast<const PagedEntryPayload&>(rhs);
    return this->mPages == typed_rhs.mPages &&
        this->mPayloadSize == typed_rhs.mPayloadSize &&
        this->mPageSize == typed_rhs.mPageSize;
  }

  void save(PersistedEntryProperties& properties, std::vector<PageId>& pages) const override;

public:
  PagedEntryPayload() = default;
  PagedEntryPayload(PersistedEntryProperties& properties, std::vector<PageId> pages);

  std::shared_ptr<EntryPayload> clone() const override { return std::make_shared<PagedEntryPayload>(*this); }

  size_t pageCount() const noexcept override { return mPages.size(); }
  uint64_t size() const noexcept override { return mPayloadSize; }
  std::optional<uint64_t> pageSize() const override;

  messaging::MessageSequence readPage(std::shared_ptr<PageStore> pageStore, const EntryName& name, size_t index) const override;

  static std::shared_ptr<PagedEntryPayload> Load(PersistedEntryProperties& properties, std::vector<PageId>& pages);

  rxcpp::observable<std::string> appendPage(PageStore& pageStore, const EntryName& name, uint64_t pagenr, std::shared_ptr<std::string> rawPage, uint64_t payloadSize); // returns MD5( data xxhash(data) )
};

}
