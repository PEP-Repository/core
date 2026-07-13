#pragma once

#include <pep/utils/CheckedPath.hpp>
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
    Timestamp validFrom;
    uint64_t checksumSubstitute{};
    uint64_t payloadSize{};
    bool isOriginalPayloadOwner{};
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
    Cell& cell_;
    uint64_t checksumSubstitute_;
    std::unique_ptr<EntryContent> content_;

  protected:
    EntryBase(Cell& cell, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content);

    void setContent(std::unique_ptr<EntryContent>&& content) { content_ = std::move(content); }

    // Allow move-construction (when EntryChange::commit converts the EntryChange into an Entry)...
    EntryBase(EntryBase&&) = default;
  public:
    // ... but no other copy or move operations: this class has reference semantics (as do derived classes)
    EntryBase(const EntryBase&) = delete;
    EntryBase& operator=(const EntryBase&) = delete;
    EntryBase& operator=(EntryBase&&) = delete;

    virtual ~EntryBase() noexcept = default; // Class isn't used polymorphically, but just in case

    Cell& getCell() const noexcept { return cell_; }
    uint64_t getChecksumSubstitute() const { return checksumSubstitute_; }
    FileStore& getFileStore() const noexcept;

    const std::unique_ptr<EntryContent>& content() const { return content_; }
    EntryName getName() const;

    bool isTombstone() const { return content_ == nullptr; }
    uint64_t payloadSize() const;
    bool isOriginalPayloadOwner() const;
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
    Timestamp lastEntryValidFrom_;
    bool valid_ = true;
    std::shared_ptr<PagedEntryPayload> pagedPayload_;

    explicit EntryChange(Cell& cell);
    explicit EntryChange(const Entry& overwrites);

  public:
    Timestamp getLastEntryValidFrom() const { return lastEntryValidFrom_; }

    using EntryBase::setContent;
    rxcpp::observable<std::string> appendPage(std::shared_ptr<std::string> rawPage, uint64_t payloadSize, uint64_t pagenr); // returns MD5( data xxhash(data) )

    // must be on same thread as FileStore
    void commit(Timestamp availableFrom) &&; // mark this entry as finished (moving it from a tmp directory to the real directory structure)
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
    static const std::string FileExtension;

    Timestamp validFrom_;

    Entry(EntryChange&& source, Timestamp validFrom);
    Entry(Cell& cell, Timestamp validFrom, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content);

    CheckedPath getFilePath(const std::string& extension) const;

  public:
    std::unique_ptr<EntryContent> cloneContent() const;

    Timestamp getValidFrom() const noexcept { return validFrom_; }
    EntryHeader header() const;
    messaging::MessageSequence readPage(size_t index);

    void save() const;
    static std::shared_ptr<Entry> TryLoad(Cell& cell, const CheckedPath& path);
    static std::shared_ptr<Entry> Load(Cell& cell, Timestamp timestamp);
  };

  using EntrySet = typename PropertyBasedContainer<std::shared_ptr<Entry>, &Entry::getValidFrom>::set;

  class Cell {
  private:
    Participant& participant_;
    const std::string& columnName_; // reference to unique string in FileStore::columnNames_
    EntryHeaders entryHeaders_;
    std::shared_ptr<Entry> latest_;

  public:
    Cell(Participant& participant, const std::string& columnName, bool load = false);

    Participant& getParticipant() const { return participant_; }
    const std::string& columnName() const noexcept { return columnName_; }

    EntryName entryName() const;
    CheckedPath path() const;

    const EntryHeaders& entryHeaders() const noexcept { return entryHeaders_; }
    void getMetrics(size_t& entryCount, uint64_t& totalPayloadBytes, uint64_t& rollingPayloadBytes) const;
    void addEntry(std::shared_ptr<Entry> entry);
    std::shared_ptr<Entry> lookup(Timestamp validAt = Timestamp::max()); // (Absent or) max value indicates "latest version"
  };

  class Participant {
  private:
    FileStore& store_;
    std::string name_; // text representation of the local SF pseudonym
    PropertyBasedContainer<std::unique_ptr<Cell>, &Cell::columnName>::set cells_;

    Cell* getCell(const std::string& columnName) const;
    Cell& provideCell(const std::string& columnName);

  public:
    Participant(FileStore& store, std::string name, bool load = false);

    FileStore& getFileStore() const noexcept { return store_; }
    const std::string& name() const noexcept { return name_; }

    CheckedPath path() const;

    std::shared_ptr<EntryChange> createEntry(const std::string& columnName) { return EntryChange::Create(this->provideCell(columnName)); }

    void getMetrics(size_t& entryCount, uint64_t& totalPayloadBytes, uint64_t& rollingPayloadBytes, const std::set<std::string>& columns) const;
    void forEachEntryHeader(const std::function<void(const EntryHeader&)>& callback) const;
    EntrySet lookupWithHistory(const std::string& column) const;
    std::shared_ptr<Entry> lookup(const std::string& column, Timestamp validAt = Timestamp::max());
  };


  EntrySet lookupWithHistory(const EntryName& name) const;
  std::shared_ptr<Entry> lookup(const EntryName& name, Timestamp validAt = Timestamp::max());
  std::shared_ptr<EntryChange> modifyEntry(const EntryName& name, bool createIfNeeded = false);

  EntryContent::Metadata makeMetadataMap(const std::map<std::string, MetadataXEntry>& xentries);
  std::map<std::string, MetadataXEntry> extractMetadataMap(const EntryContent::Metadata& metadata);

  void getMetrics(size_t& entryCount, uint64_t& totalPayloadBytes, uint64_t& rollingPayloadBytes, const std::set<std::string>& columns = {}) const;
  void forEachEntryHeader(const std::function<void(const EntryHeader&)>& callback) const;

  const CheckedPath& metaDir() const {
    return path_;
  }

  std::set<std::string> pagePaths() const { return {}; } // TODO: implement

private:
  FileStore(
    const std::filesystem::path& metadatapath,
    const Configuration& pageStoreConfig,
    std::shared_ptr<boost::asio::io_context> io_context,
    std::shared_ptr<prometheus::Registry> metrics_registry);

  // Keep collections of unique strings to save memory: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2322 .
  // Note that "No iterators or references are invalidated" when an std::set or std::map changes, so
  // we can store and use references to these strings as long as we don't discard them from the collection.
  std::set<std::string> columnNames_;
  std::map<std::string, std::set<std::string>> metadataValues_;

  PropertyBasedContainer<std::unique_ptr<Participant>, &Participant::name>::set participants_;
  CheckedPath path_;
  std::shared_ptr<PageStore> pagestore_;

  const std::string& getColumnString(const std::string& value);
  EntryContent::MetadataEntry makeMetadataEntry(std::string key, std::string value);

  Participant* getParticipant(const std::string& name) const;
  Participant& provideParticipant(const std::string& name);
};

}
