#pragma once

#include <pep/utils/Shared.hpp>
#include <pep/storagefacility/EntryContent.hpp>
#include <pep/messaging/MessageSequence.hpp>

#include <filesystem>
#include <string>
#include <memory>

namespace pep {

/**
 * directory structure:
 * - .1, .2, .3 are pages
 * - name.mtime.entry is a snapshot of all the metadata and pages that are used
 */
class FileStore : public SharedConstructor<FileStore> {
  friend class SharedConstructor<FileStore>;
  friend class EntryContent;

public:
  class Participant;
  class Cell;
  class Entry;

  struct EntryHeader {
    EpochMillis validFrom;
    uint64_t checksumSubstitute;
  };
  using EntryHeaders = typename PropertyBasedContainer<EntryHeader, &EntryHeader::validFrom>::set;

  /*!
   * \brief Utility base class for Entry and EntryChange classes (defined below).
   * \remark Ensures that appropriate values are copied when we
   *         - create an EntryChange on the basis of an existing Entry, i.e. when preparing a cell update.
   *         - create an Entry on the basis of an EntryChange, i.e. when committing the EntryChange.
   */
  class EntryBase {
  private:
    Cell& mCell;
    uint64_t mChecksumSubstitute;
    std::unique_ptr<EntryContent> mContent;

  protected:
    EntryBase(Cell& cell, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content);

    void setContent(std::unique_ptr<EntryContent>&& content) { mContent = std::move(content); }

    // Allow move-construction (when EntryChange::commit converts the EntryChange into an Entry)...
    EntryBase(EntryBase&&) = default;
  public:
    // ... but no other copy or move operations: this class has reference semantics (as do derived classes)
    EntryBase(const EntryBase&) = delete;
    EntryBase& operator=(const EntryBase&) = delete;
    EntryBase& operator=(EntryBase&&) = delete;

    virtual ~EntryBase() noexcept = default; // Class isn't used polymorphically, but just in case

    Cell& getCell() const noexcept { return mCell; }
    uint64_t getChecksumSubstitute() const { return mChecksumSubstitute; }
    FileStore& getFileStore() const noexcept;

    const std::unique_ptr<EntryContent>& content() const { return mContent; }
    EntryName getName() const;

    bool isTombstone() const { return mContent == nullptr; }
  };

  /*!
   * \brief Represents a pending update to a cell. A new Entry will be created when an EntryChange is commit()ted.
   */
  class EntryChange : public EntryBase, public std::enable_shared_from_this<EntryChange>, private SharedConstructor<EntryChange> {
    // Allow SharedConstructor<EntryChange>::Create to access our private constructor(s)
    friend class SharedConstructor<EntryChange>;
    // Allow FileStore to invoke SharedConstructor<EntryChange>::Create (which we inherit privately)
    friend class FileStore;

  private:
    EpochMillis mLastEntryValidFrom = 0;
    bool mValid = true;
    std::shared_ptr<PagedEntryPayload> mPagedPayload;

    explicit EntryChange(Cell& cell);
    explicit EntryChange(const Entry& overwrites);

  public:
    const EpochMillis& getLastEntryValidFrom() const { return mLastEntryValidFrom; }

    void setContent(std::unique_ptr<EntryContent>&& content) { EntryBase::setContent(std::move(content)); }
    rxcpp::observable<std::string> appendPage(std::shared_ptr<std::string> rawPage, uint64_t payloadSize, uint64_t pagenr); // returns MD5( data xxhash(data) )

    // must be on same thread as FileStore
    void commit(EpochMillis availableFrom) &&; // mark this entry as finished (moving it from a tmp directory to the real directory structure)
    void cancel() &&;
  };

  /*!
   * \brief Represents a cell version ("data card").
   */
  class Entry : public EntryBase, public std::enable_shared_from_this<Entry>, private SharedConstructor<Entry>
  {
    // Allow SharedConstructor<Entry>::Create to access our private constructor(s)
    friend class SharedConstructor<Entry>;
    // Allow EntryChange to invoke SharedConstructor<Entry>::Create (which we inherit privately)
    friend class EntryChange;

  private:
    static const std::string FILE_EXTENSION;

    EpochMillis mValidFrom;

    Entry(EntryChange&& source, EpochMillis validFrom);
    Entry(Cell& cell, EpochMillis validFrom, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content);

    std::filesystem::path getFilePath(const std::string& extension) const;

  public:
    std::unique_ptr<EntryContent> cloneContent() const;

    EpochMillis getValidFrom() const noexcept { return mValidFrom; }
    EntryHeader header() const;
    messaging::MessageSequence readPage(size_t index);

    void save() const;
    static std::shared_ptr<Entry> TryLoad(Cell& cell, const std::filesystem::path& path);
    static std::shared_ptr<Entry> Load(Cell& cell, EpochMillis timestamp);
  };

  using EntrySet = typename PropertyBasedContainer<std::shared_ptr<Entry>, &Entry::getValidFrom>::set;

  class Cell {
  private:
    Participant& mParticipant;
    const std::string& mColumnName; // reference to unique string in FileStore::mColumnNames
    EntryHeaders mEntryHeaders;
    std::shared_ptr<Entry> mLatest;

  public:
    Cell(Participant& participant, const std::string& columnName, bool load = false);

    Participant& getParticipant() const { return mParticipant; }
    const std::string& columnName() const noexcept { return mColumnName; }

    EntryName entryName() const;
    std::filesystem::path path() const;

    const EntryHeaders& entryHeaders() const noexcept { return mEntryHeaders; }
    void addEntry(std::shared_ptr<Entry> entry);
    std::shared_ptr<Entry> lookup(EpochMillis validAt = std::numeric_limits<EpochMillis>::max()); // (Absent or) max value indicates "latest version"
  };

  class Participant {
  private:
    FileStore& mStore;
    std::string mName; // text representation of the local SF pseudonym
    PropertyBasedContainer<std::unique_ptr<Cell>, &Cell::columnName>::set mCells;

    Cell* getCell(const std::string& columnName) const;
    Cell& provideCell(const std::string& columnName);

  public:
    Participant(FileStore& store, std::string name, bool load = false);

    FileStore& getFileStore() const noexcept { return mStore; }
    const std::string& name() const noexcept { return mName; }

    std::filesystem::path path() const;

    std::shared_ptr<EntryChange> createEntry(const std::string& columnName) { return EntryChange::Create(this->provideCell(columnName)); }

    size_t entryCount() const;
    void forEachEntryHeader(const std::function<void(const EntryHeader&)>& callback) const;
    EntrySet lookupWithHistory(const std::string& column) const;
    std::shared_ptr<Entry> lookup(const std::string& column, EpochMillis validAt = std::numeric_limits<EpochMillis>::max());
  };


  EntrySet lookupWithHistory(const EntryName& name) const;
  std::shared_ptr<Entry> lookup(const EntryName& name, EpochMillis validAt = std::numeric_limits<EpochMillis>::max());
  std::shared_ptr<EntryChange> modifyEntry(const EntryName& name, bool createIfNeeded = false);

  EntryContent::Metadata makeMetadataMap(const std::map<std::string, MetadataXEntry>& xentries);
  std::map<std::string, MetadataXEntry> extractMetadataMap(const EntryContent::Metadata& metadata);

  size_t entryCount() const;
  void forEachEntryHeader(const std::function<void(const EntryHeader&)>& callback) const;

  const std::filesystem::path& metaDir() const {
    return mPath;
  }

private:
  FileStore(
    const std::filesystem::path& metadatapath,
    std::shared_ptr<Configuration> pageStoreConfig,
    std::shared_ptr<boost::asio::io_context> io_context,
    std::shared_ptr<prometheus::Registry> metrics_registry);

  // Keep collections of unique strings to save memory: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2322 .
  // Note that "No iterators or references are invalidated" when an std::set or std::map changes, so
  // we can store and use references to these strings as long as we don't discard them from the collection.
  std::set<std::string> mColumnNames;
  std::map<std::string, std::set<std::string>> mMetadataValues;

  const std::string& getColumnString(const std::string& value);
  EntryContent::MetadataEntry makeMetadataEntry(std::string key, std::string value);

  Participant* getParticipant(const std::string& name) const;
  Participant& provideParticipant(const std::string& name);

  PropertyBasedContainer<std::unique_ptr<Participant>, &Participant::name>::set mParticipants;
  std::filesystem::path mPath;
  std::shared_ptr<PageStore> mPagestore;
};

}
