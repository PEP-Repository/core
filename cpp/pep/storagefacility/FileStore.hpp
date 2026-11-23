#pragma once

#include <pep/async/ExponentialBackoff.hpp>
#include <pep/async/IoContext_fwd.hpp>
#include <pep/utils/CheckedPath.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/server/ChecksumChain.hpp>
#include <pep/storagefacility/EntryContent.hpp>
#include <pep/storagefacility/SqliteStorage.hpp>
#include <pep/messaging/MessageSequence.hpp>

#include <filesystem>
#include <string>
#include <memory>

namespace pep {

/// directory structure:
/// - .1, .2, .3 are pages
/// - name.mtime.entry is a snapshot of all the metadata and pages that are used
class FileStore : public SharedConstructor<FileStore>, public std::enable_shared_from_this<FileStore> {
  friend class SharedConstructor<FileStore>;
  friend class EntryContent;

public:
  class Participant;
  class Cell;
  class Entry;

  struct Metrics {
    size_t entryCount{};
    uint64_t totalBytes{}; // including redactedBytes
    uint64_t redactedBytes{};
    uint64_t rollingBytes{};

    Metrics& operator+=(const Metrics& rhs);
  };

  /// \brief Utility base class for Entry and EntryChange classes (defined below).
  /// \remark Ensures that appropriate values are copied when we
  ///         - create an EntryChange on the basis of an existing Entry, i.e. when preparing a cell update.
  ///         - create an Entry on the basis of an EntryChange, i.e. when committing the EntryChange.
  class EntryBase {
  private:
    Cell& cell_;
    uint64_t checksumSubstitute_;
    std::unique_ptr<EntryContent> content_;

  protected:
    EntryBase(Cell& cell, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content);

    void setContent(std::unique_ptr<EntryContent>&& content) { content_ = std::move(content); }
    std::set<std::string> getPagePaths() const;

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
    FileStore& getFileStore() noexcept;

    const std::unique_ptr<EntryContent>& content() const { return content_; }
    EntryName getName() const;

    bool isTombstone() const { return content_ == nullptr; }
    uint64_t payloadSize() const;
    std::optional<Timestamp> originalPayloadEntryTimestamp() const;
  };

  /// \brief Represents a pending update to a cell. A new Entry will be created when an EntryChange is commit()ted.
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

  /// \brief Represents a cell version ("data card").
  class Entry : public EntryBase, public std::enable_shared_from_this<Entry>, private SharedConstructor<Entry>
  {
    // Allow SharedConstructor<Entry>::Create to access our private constructor(s)
    friend class SharedConstructor<Entry>;
    // Allow EntryChange to invoke SharedConstructor<Entry>::Create (which we inherit privately)
    friend class EntryChange;

  private:
    static const std::string FileExtension;

    Timestamp validFrom_;
    std::optional<Timestamp> redactedAt_;

    Entry(EntryChange&& source, Timestamp validFrom);
    Entry(Cell& cell, Timestamp validFrom, std::optional<Timestamp> redactedAt, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content);

    CheckedPath getFilePath(const std::string& extension) const;

  public:
    std::unique_ptr<EntryContent> cloneContent() const;

    Timestamp getValidFrom() const noexcept { return validFrom_; }

    std::optional<Timestamp> getRedactedAt() const { return redactedAt_; }
    void markRedacted(Timestamp at) noexcept; // in memory
    void throwErrorIfRedacted(const std::string& logContext) const;
    std::set<std::string> pagePaths() const;

    messaging::MessageSequence readPage(size_t index);

    void save() const;
    static std::shared_ptr<Entry> TryLoad(Cell& cell, const CheckedFileName& filename);
    static std::shared_ptr<Entry> Load(Cell& cell, Timestamp timestamp);
    static void EnsureRedactionPersisted(Timestamp redactedAt, Cell& cell, Timestamp version); // on disk
  };

  struct CellVersion {
    Timestamp validFrom;
    uint64_t checksumSubstitute{};
    uint64_t payloadSize{};
    std::optional<Timestamp> redactedAt;
    std::optional<Timestamp> originalPayloadEntryTimestamp{};
    bool isTombstone{};

    static CellVersion FromEntry(const Entry& entry);
  };
  using CellVersions = typename PropertyBasedContainer<CellVersion, &CellVersion::validFrom>::set;

  class Cell {
  private:
    Participant& participant_;
    const std::string& columnName_; ///< reference to unique string in FileStore::columnNames_
    CellVersions versions_;
    std::shared_ptr<Entry> latest_; ///< May be nullptr, e.g. when a store is canceled

  public:
    Cell(Participant& participant, const std::string& columnName, bool load = false);

    Participant& participant() { return participant_; }
    const Participant& participant() const { return participant_; }
    const std::string& columnName() const noexcept { return columnName_; }
    const CellVersions& versions() const noexcept { return versions_; }

    EntryName entryName() const;
    CheckedPath path() const;

    Metrics getMetrics() const;
    void addEntry(std::shared_ptr<Entry> entry);
    std::shared_ptr<Entry> lookup(Timestamp validAt = Timestamp::max()); // (Absent or) max value indicates "latest version"

    std::set<std::string> pagePaths() const; // for latest version

    using VersionTimestamps = std::set<Timestamp>;
    void markRedacted(Timestamp redactedAt, const VersionTimestamps& cellVersions) noexcept; // in memory
    void ensureRedactionPersisted(Timestamp redactedAt, Timestamp cellVersion); // on disk
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

    FileStore& fileStore() noexcept { return store_; }
    const FileStore& fileStore() const noexcept { return store_; }
    const std::string& name() const noexcept { return name_; }
    PropertyBasedContainer<const Cell*, &Cell::columnName>::set cells() const;

    CheckedPath path() const;

    std::shared_ptr<EntryChange> createEntry(const std::string& columnName) { return EntryChange::Create(this->provideCell(columnName)); }

    Metrics getMetrics(const std::set<std::string>& columns) const;
    std::shared_ptr<Entry> lookup(const std::string& column, Timestamp validAt = Timestamp::max());

    std::set<std::string> pagePaths() const; // for latest version

    using VersionTimestampsByColumn = std::unordered_map<std::string, Cell::VersionTimestamps>;
    void markRedacted(Timestamp redactedAt, const VersionTimestampsByColumn& vsByColumn) noexcept; // in memory
    void ensureRedactionPersisted(Timestamp redactedAt, const std::string& column, Timestamp version); // on disk
  };

private:
  /// \brief Constructor
  /// \param sqlite (A shared_ptr to) the SqliteStorage instance associated with this file store. Caller must ensure that the reference remains valid throughout this instance's lifetime.
  /// \param metadatapath The path to the file store's "meta" directory, where it can store its (subdirectories and) files.
  /// \param pageStoreConfig The configuration for the backing page store.
  /// \param io_context The I/O context associated with asynchronous code for the calling thread.
  /// \param metrics_registry The (optional) metrics registry where this instance may register its metrics
  FileStore(
    std::shared_ptr<storagefacility::SqliteStorage> sqlite,
    const std::filesystem::path& metadatapath,
    const Configuration& pageStoreConfig,
    std::shared_ptr<boost::asio::io_context> io_context,
    std::shared_ptr<prometheus::Registry> metrics_registry);

  std::shared_ptr<boost::asio::io_context> ioContext_;
  std::shared_ptr<storagefacility::SqliteStorage> sqlite_;
  std::thread redactionPersistence_;
  std::mutex redactionPersistenceMux_; // Synchronizes access to the redactionPersistence_ instance
  ExponentialBackoff redactionPersistenceBackoff_;
  std::recursive_mutex fileSystemMux_; // Synchronizes access to (the meta directory on) the file system

  // Keep collections of unique strings to save memory: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2322 .
  // Note that "No iterators or references are invalidated" when an std::set or std::map changes, so
  // we can store and use references to these strings as long as we don't discard them from the collection.
  std::set<std::string> columnNames_;
  std::map<std::string, std::set<std::string>> metadataValues_;

  PropertyBasedContainer<std::unique_ptr<Participant>, &Participant::name>::set participants_;
  CheckedPath path_;
  std::shared_ptr<PageStore> pagestore_;

  ChecksumChains checksumChains_;

  const std::string& getColumnString(const std::string& value);
  EntryContent::MetadataEntry makeMetadataEntry(std::string key, std::string value);

  Participant* getParticipant(const std::string& name) const;
  Participant& provideParticipant(const std::string& name);

  void markRedacted(const RedactionCycle& cycle) noexcept;
  void persistRedactionCycleAsync(RedactionCycle cycle); // Entry point for the redactionPersistence_ thread
  void persistRemainingPendingRedactions(bool previous_attempt_failed);

public:
  PropertyBasedContainer<const Participant*, &Participant::name>::set participants() const;

  std::shared_ptr<Entry> lookup(const EntryName& name, Timestamp validAt = Timestamp::max());

  /// \brief Pends a new version in a cell
  /// \param name The (name of) the entry/cell where the new version should be created
  /// \param requireExisting Whether a previous version must exist (and not be a tombstone)
  /// \param requireUnredacted Whether the previous version (if it exists) must be un-redacted
  /// \return (A non-null shared_ptr to) an EntryChange for the specified cell
  /// \throws An Error if the current/latest cell state doesn't satisfy the specified requirements
  std::shared_ptr<EntryChange> modifyEntry(const EntryName& name, bool requireExisting = true, bool requireUnredacted = true);

  EntryContent::Metadata makeMetadataMap(const std::map<std::string, MetadataXEntry>& xentries);
  std::map<std::string, MetadataXEntry> extractMetadataMap(const EntryContent::Metadata& metadata);

  Metrics getMetrics(const std::set<std::string>& columns = {}) const;

  const CheckedPath& metaDir() const {
    return path_;
  }

  std::set<std::string> pagePaths() const; // for latest version

  void applyRedactionCycle(const RedactionCycle& cycle); // synchronously in memory and asynchronously on disk
  void persistPendingRedactions(); // asynchronously on disk

  const ChecksumChains& checksumChains() const noexcept { return checksumChains_; }
};

}
