#include <pep/storagefacility/FileStore.hpp>

#include <pep/async/IoContext_fwd.hpp>
#include <pep/messaging/MessageSequence.hpp>
#include <pep/morphing/Metadata.hpp>
#include <pep/morphing/MorphingSerializers.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/serialization/Error.hpp>
#include <pep/serialization/Serialization.hpp>
#include <pep/storagefacility/Constants.hpp>
#include <pep/storagefacility/EntryContent.hpp>
#include <pep/storagefacility/EntryName.hpp>
#include <pep/storagefacility/EntryPayload.hpp>
#include <pep/storagefacility/PageStore.hpp>
#include <pep/storagefacility/PersistedEntryProperties.hpp>
#include <pep/storagefacility/PrefixedStrings.hpp>
#include <pep/storagefacility/RedactionCycle.hpp>
#include <pep/storagefacility/SfChecksumChain.hpp>
#include <pep/storagefacility/SqliteStorage.hpp>
#include <pep/utils/BuildFlavor.hpp>
#include <pep/utils/CheckedPath.hpp>
#include <pep/utils/Configuration_fwd.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Generator.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/PropertyBasedContainer.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Raw.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/utils/Timestamp.hpp>
#include <pep/utils/TypeTraits.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <iterator> // IWYU pragma: keep (used in assert)
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rxcpp/rx-lite.hpp>

using namespace std::chrono;
using namespace std::literals;

namespace pep {

namespace {

const std::string ChecksumSubstituteKey = "checksum-substitute";
const std::string RedactedAtKey = "redacted-at";

uint64_t GenerateChecksumSubstitute() {
  return RandomInteger<uint64_t>();
}

const std::string EntryFileType("pepentry");
const std::string LogTag("StorageFacility");

}

// Design:
// - metadata is stored on local file system
// - pages is stored on 'data' volume (which can be migrated to the cloud later on rather easy)
// - every stored item is xxhashed so it can be verified that no error occured
// - on start all the metadata is loaded into memory (4 KiB per entry x 256k entries = 1 GiB of RAM)
// - I/O model: all reads are from memory; writes will synchronous write to disk for consistency
// - retrieving 40k items out of 360k items (no historical items) cost 92ms
// - retrieving 2 latest items out of 40k historical items of a total of 360k items cost 99ms

 // Challenges:
 // - correctly (with all error condition) retrieve data from S3 interface
 // - if there are many entries; starting will take longer (possible migrate to mmap()ed data structure)
 // - partitioning (within a host; but also mutliple storage facilities)

const std::string FileStore::Entry::FileExtension = ".entry";

FileStore::Metrics& FileStore::Metrics::operator+=(const Metrics& rhs) {
  entryCount += rhs.entryCount;
  totalBytes += rhs.totalBytes;
  redactedBytes += rhs.redactedBytes;
  rollingBytes += rhs.rollingBytes;

  return *this;
}

EntryContent::MetadataEntry FileStore::makeMetadataEntry(std::string key, std::string value) {
  std::lock_guard lock(metadataValuesMux_);
  auto pos = metadataValues_.emplace(std::move(key), std::set<std::string>()).first;
  auto valuePos = pos->second.emplace(std::move(value)).first;

  auto keyPointer = &pos->first;
  auto valuePointer = &*valuePos;

  return EntryContent::MetadataEntry(keyPointer, valuePointer);
}

EntryContent::Metadata FileStore::makeMetadataMap(const std::map<std::string, MetadataXEntry>& xentries) {
  EntryContent::Metadata result;
  for (const auto& [key, value] : xentries) {
    auto entry = this->makeMetadataEntry(key, Serialization::ToString(value));
    [[maybe_unused]] auto emplaced = result.emplace(entry).second;
    assert(emplaced);
  }
  return result;
}

std::map<std::string, MetadataXEntry> FileStore::extractMetadataMap(const EntryContent::Metadata& metadata) {
  std::map<std::string, MetadataXEntry> result;
  for (const auto& [key, value] : metadata) {
    auto xentry = Serialization::FromString<MetadataXEntry>(*value);
    [[maybe_unused]] auto emplaced = result.emplace(*key, xentry).second;
    assert(emplaced);
  }
  return result;
}

FileStore::Participant* FileStore::getParticipant(const std::string& name) const {
  auto pos = participants_.find(name);
  if (pos == participants_.cend()) {
    return nullptr;
  }
  return &**pos;
}

FileStore::Participant& FileStore::provideParticipant(const std::string& name) {
  auto existing = this->getParticipant(name);
  if (existing != nullptr) {
    return *existing;
  }

  return **participants_.emplace(std::make_unique<Participant>(*this, name)).first;
}

void FileStore::markRedacted(const RedactionCycle& cycle) noexcept {
  // Method is "noexcept" so that it can't produce a state inconsistent with the "cycle", which has been "applied"
  // (and presumably stored in SqliteStorage).
  assert(cycle.applied);

  std::unordered_map<std::string, Participant::VersionTimestampsByColumn> byParticipant;
  for (const auto& spec : sqlite_->getIncludedCellVersions(cycle.id)) {
    [[maybe_unused]] auto emplaced = byParticipant[spec.cell.participant()][spec.cell.column()].emplace(spec.timestamp).second;
    assert(emplaced);
  }
  for (const auto& p : byParticipant) {
    auto& [name, byColumn] = p;
    auto participant = this->getParticipant(name);
    assert(participant != nullptr);
    participant->markRedacted(*cycle.applied, byColumn);
  }
}

void FileStore::persistRedactionBatchAsync(std::vector<EntryRedactionPersister> persisters) {
  // Entry point for the redactionPersistence_ thread

  auto failed = false;
  try {
    for (const auto& persister : persisters) {
      persister();
    }

    assert(persistingRedactionCycle_.has_value());
    assert(!persistingRedactionCycle_->completed.has_value());
    persistingRedactionCycle_->completed = TimeNow();
    sqlite_->updateRedactionCycle(*persistingRedactionCycle_);
    PEP_LOG(LogTag, Severity::Info) << "Completed persistence of redaction cycle " << persistingRedactionCycle_->id;
    persistingRedactionCycle_.reset();
  }
  catch (...) {
    // Eat all exceptions to prevent the thread from taking down the entire process
    PEP_LOG(LogTag, Severity::Error) << "Encountered exception persisting redactions: " << GetExceptionMessage(std::current_exception());
    // Since the cycle hasn't been marked "completed" in sqlite_, we'll (let the ASIO thread) retry with a new redactionPersistence_ thread
    failed = true;
  }

  post(*ioContext_, [self = SharedFrom(*this), failed]() { // Let the ASIO thread know that we finished processing
      self->handleRedactionBatchPersistenceFinished(failed);
    });
}

const std::string& FileStore::getColumnString(const std::string& value) {
  if (value.contains(EntryName::Delimiter)) {
    throw std::runtime_error("Cell name may not contain an entry name delimiter");
  }
  return *columnNames_.insert(value).first;
}

FileStore::EntryRedactionPersister::EntryRedactionPersister(Timestamp redactAt, Cell& cell, Timestamp version)
  : redactAt_(redactAt), cell_(cell), version_(version) {
}

bool FileStore::EntryRedactionPersister::operator()() const {
  return Entry::EnsureRedactionPersisted(redactAt_, cell_, version_);
}

FileStore::FileStore(
  std::shared_ptr<storagefacility::SqliteStorage> sqlite,
  const std::filesystem::path& metadatapath,
  const Configuration& pageStoreConfig,
  std::shared_ptr<boost::asio::io_context> io_context,
  std::shared_ptr<prometheus::Registry> metrics_registry)
  : ioContext_(io_context),
  sqlite_(sqlite),
  redactionPersistenceBackoff_(*ioContext_),
  path_(CheckedPath::FromTrusted(metadatapath)),
  pagestore_(PageStore::Create(io_context, metrics_registry, pageStoreConfig)),
  checksumChains_(SfChecksumChain::CreateAll(*this))
{
  // throws when an error occurs while creating any of the given directories in the supplied path
  std::filesystem::create_directories(path_);

  // Load cell versions from disk. File system access doesn't need to be synchronized because our redaction persistence thread isn't running.
  auto start_time = steady_clock::now();
  for (const auto& p : std::filesystem::directory_iterator(path_)) {
    auto name = p.path().filename().string();
    if (std::filesystem::is_directory(p.path()) && name.size() == LocalPseudonym::TextLength()) {
      [[maybe_unused]] auto emplaced = participants_.emplace(std::make_unique<Participant>(*this, name, true)).second;
      assert(emplaced);
    }
  }

  // Mark versions redacted that are included in redaction cycles that have been applied (confirmed) but not (completely) persisted yet
  for (auto cycle : sqlite_->getRedactionCycles() | std::views::filter([](const RedactionCycle& cycle) {return cycle.applied && !cycle.completed; })) {
    this->markRedacted(cycle);
  }

  auto entryCount = this->getMetrics().entryCount;

  duration<double> seconds(steady_clock::now() - start_time);
  std::ostringstream message;
  message.setf(std::ios::fixed);
  message.precision(2);
  message << "Loaded " << entryCount << " file store entries in " << seconds;
  if (seconds != decltype(seconds)::zero()) {
    message << " (" << (static_cast<double>(entryCount) / seconds.count()) << " entries per second)";
  }
  PEP_LOG(LogTag, Severity::Info) << message.str();
}

FileStore::Participant::Participant(FileStore& store, std::string name, bool load)
  : store_(store), name_(std::move(name)) {
  std::lock_guard lock(store.fileSystemMux_);
  if (load) {
      for (const auto& p : std::filesystem::directory_iterator(this->path())) {
        if (p.is_directory()) {
          cells_.emplace(std::make_unique<Cell>(*this, p.path().filename().string(), true));
        }
      }
  }
  else {
    // throws when an error occurs while creating any of the given directories in the supplied path
    std::filesystem::create_directories(this->path());
  }
}

FileStore::CellVersion FileStore::CellVersion::FromEntry(const Entry& entry) {
  return CellVersion{
    .validFrom = entry.getValidFrom(),
    .checksumSubstitute = entry.getChecksumSubstitute(),
    .payloadSize = entry.payloadSize(),
    .redactedAt = entry.getRedactedAt(),
    .originalPayloadEntryTimestamp = entry.originalPayloadEntryTimestamp(),
    .isTombstone = entry.isTombstone(),
  };
}

FileStore::Cell::Cell(Participant& participant, const std::string& columnName, bool load)
  : participant_(participant), columnName_(participant.fileStore().getColumnString(columnName)) {
  std::lock_guard lock(participant_.fileStore().fileSystemMux_);
  if (load) {
    for (const auto& i : std::filesystem::directory_iterator(this->path())) {
      if (auto entry = Entry::TryLoad(*this, CheckedFileName(i.path().filename()))) {
        this->addEntry(entry);
      }
    }
  }
  else {
    // throws when an error occurs while creating any of the given directories in the supplied path
    std::filesystem::create_directories(this->path());
  }
}

FileStore::Metrics FileStore::getMetrics(const std::set<std::string>& columns) const {
  Metrics result;
  for (const auto& participant : participants_) {
    result += participant->getMetrics(columns);
  }
  return result;
}


PropertyBasedContainer<const FileStore::Participant*, &FileStore::Participant::name>::set FileStore::participants() const {
  PropertyBasedContainer<const FileStore::Participant*, &FileStore::Participant::name>::set result;
  for (const auto& participant : participants_) {
    [[maybe_unused]] auto emplaced = result.emplace(participant.get());
    assert(emplaced.second);
    assert(std::next(emplaced.first) == result.end()); // Should have been inserted at the back of the result set
  }
  return result;
}

Generator<PrefixedStringsEntry> FileStore::pagePaths() const {
  // Page paths are "<participant>/<column>/<page>": produce participant and column as (shared) prefixes
  static_assert(DerivedFromSpecialization<decltype(participants_), std::set>, "Iterators need to remain valid when participants are added");
  for (const auto& participant : participants_) {
    co_yield PushPrefix{.prefix = std::format("{}{}", participant->name(), EntryName::Delimiter)};
    for (auto&& entry : participant->pagePaths()) {
      co_yield std::move(entry);
    }
    co_yield PopPrefix{};
  }
}

void FileStore::persistPendingRedactions() {
  // Let the persistence thread continue processing if it's already running
  if (redactionPersistence_.joinable()) {
    return;
  }

  auto pending = sqlite_->getRedactionCycles()
    | std::views::filter([](const RedactionCycle& cycle) { return cycle.applied && !cycle.completed; })
    | std::ranges::to<std::vector>();

  // Do nothing if there are no redaction cycles that need persisting
  if (pending.empty()) {
    return;
  }

  // Find the oldest unpersisted cycle...
  auto i = std::ranges::min_element(pending, {}, &RedactionCycle::applied);
  assert(i != pending.end());
  persistingRedactionCycle_ = *i;
  auto redactAt = *persistingRedactionCycle_->applied;

  // ... and batch (some of) its entries to...
  auto persisters = sqlite_->getIncludedCellVersions(i->id)
    | std::views::transform([this, redactAt](const storagefacility::SqliteStorage::CellVersionSpec& spec) {
        return this->getParticipant(spec.cell.participant())->makeRedactionPersister(redactAt, spec.cell.column(), spec.timestamp);
      })
    | std::ranges::to<std::vector>();

  // ... start a (new) persistence thread now to process them
  redactionPersistence_ = std::thread([self = SharedFrom(*this), persisters]() { self->persistRedactionBatchAsync(persisters); });
}

void FileStore::handleRedactionBatchPersistenceFinished(bool failed) {
  // Join the redactionPersistence_ thread to clean up
  assert(redactionPersistence_.joinable());
  redactionPersistence_.join();
  assert(!redactionPersistence_.joinable());

  if (failed) {
    assert(persistingRedactionCycle_.has_value());
    redactionPersistenceBackoff_.retry([self = SharedFrom(*this)](const boost::system::error_code& error) {
        if (!error) {
          self->persistPendingRedactions();
        }
      });
  }
  else {
    assert(!persistingRedactionCycle_.has_value());
    redactionPersistenceBackoff_.success();
    this->persistPendingRedactions();
  }
}

void FileStore::applyRedactionCycle(const RedactionCycle& cycle) {
  this->markRedacted(cycle);
  this->persistPendingRedactions();
}

std::shared_ptr<FileStore::Entry> FileStore::lookup(const EntryName& name, Timestamp validAt) {
  auto participant = this->getParticipant(name.participant());
  if (participant == nullptr) {
    return nullptr;
  }
  return participant->lookup(name.column(), validAt);
}

std::shared_ptr<FileStore::Entry> FileStore::Cell::lookup(Timestamp validAt) {
  // The std::map<>::lower_bound function will find the entry _after_ the one we need when validAt == Timestamp::max().
  // So to make the function produce consistent results, we search for "validAt+1" to ensure that we _always_ find the entry after the one we need.
  auto find = validAt == Timestamp::max() ? Timestamp::max() : validAt + 1ms;
  auto it = versions_.lower_bound(find);

  // If we're positioned on the first item, the request was for a "validAt" before the first entry was stored.
  if (it == versions_.begin())
    return nullptr;

  // Since we're positioned after the item we're interested in, we skip back.
  --it;

  assert(latest_ != nullptr);
  if (it->validFrom == latest_->getValidFrom()) {
    return latest_;
  }

  std::lock_guard lock(this->participant().fileStore().fileSystemMux_);
  auto result = Entry::Load(*this, it->validFrom);
  if (auto redactedAt = it->redactedAt) {
    if (!result->getRedactedAt()) { // If the .entry file didn't contain a "redacted-at" (yet) but it's included in a pending (i.e. applied/confirmed) redaction cycle:
      result->markRedacted(*redactedAt);
    }
  }
  return result;
}

Generator<PrefixedStringsEntry> FileStore::Cell::pagePaths() const {
  // Keep the entry alive while we iterate over its pages, even if it's superseded (and discarded) in the meantime
  auto latest = latest_;
  if (latest == nullptr) { // E.g. if a store was canceled
    co_return;
  }
  // These are unique, because page names within a version are unique,
  // and we're only including a single version.
  for (auto&& path : latest->pageNames()) {
    co_yield PrefixedString{.string = std::move(path)};
  }
}

void FileStore::Cell::markRedacted(Timestamp redactedAt, const VersionTimestamps& cellVersions) noexcept {
  // A set<> only provides read-only access to its items, so we can't update (entries in) our versions_ in place.
  // So instead we create a new set<> with updated items and overwrite our entire versions_ member.
  // No need to synchronize because the redaction persistence thread doesn't touch (data derived from) our versions_
  // field.
  CellVersions updated;

  for (auto version : versions_) {
    if (!version.redactedAt.has_value()) {
      if (cellVersions.contains(version.validFrom)) {
        version.redactedAt = redactedAt;
        if (version.validFrom == latest_->getValidFrom()) {
          latest_->markRedacted(*version.redactedAt);
        }
      }
    }
    [[maybe_unused]] auto emplaced = updated.emplace(std::move(version)).second;
    assert(emplaced);
  }

  versions_ = std::move(updated);
}

std::shared_ptr<FileStore::EntryChange> FileStore::modifyEntry(const EntryName& name, bool requireExisting, bool requireUnredacted) {
  if (auto entry = lookup(name)) {
    if (requireExisting && entry->isTombstone()) {
      throw Error("Cannot update cell that has been previously cleared/deleted");
    }
    if (requireUnredacted) {
      entry->throwErrorIfRedacted("FileStore::modifyEntry");
    }

    // Create a normal EntryChange if we can
    if (!entry->getRedactedAt()) {
      return EntryChange::Create(*entry);
    }
    // else entry is redacted: let default handling (below) create a fresh EntryChange
  }
  else if (requireExisting) {
    throw Error("Cannot find cell to update");
  }

  // Entry didn't exist or was redacted: create an EntryChange that's not based on a previous version
  return this->provideParticipant(name.participant()).createEntry(name.column());
}

FileStore::EntryChange::EntryChange(Cell& cell)
  : EntryBase(cell, GenerateChecksumSubstitute(), nullptr),
    lastEntryValidFrom_{/*zero*/} {
  // We may be storing into a non-empty cell, e.g. if the latest version was redacted
  const CellVersions& versions = cell.versions();
  if (!versions.empty()) {
    lastEntryValidFrom_ = versions.rbegin()->validFrom;
  }
}

FileStore::EntryChange::EntryChange(const Entry& overwrites)
  : EntryBase(overwrites.getCell(), GenerateChecksumSubstitute(), overwrites.cloneContent()), lastEntryValidFrom_(overwrites.getValidFrom()) {
  assert(!overwrites.getRedactedAt());
}

CheckedPath FileStore::Entry::getFilePath(const std::string& extension) const {
  auto filename = std::to_string(TicksSinceEpoch<milliseconds>(this->getValidFrom())) + extension;
  return this->getCell().path() / CheckedFileName(filename);
}

FileStore::EntryBase::EntryBase(Cell& cell, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content)
  : cell_(cell), checksumSubstitute_(checksumSubstitute), content_(std::move(content)) {
}

std::unique_ptr<EntryContent> FileStore::Entry::cloneContent() const {
  const auto& content = this->content();
  if (content == nullptr) {
    return nullptr;
  }
  return std::make_unique<EntryContent>(*content, validFrom_);
}

FileStore& FileStore::EntryBase::getFileStore() noexcept {
  return cell_.participant().fileStore();
}

EntryName FileStore::EntryBase::getName() const {
  return this->getCell().entryName();
}

uint64_t FileStore::EntryBase::payloadSize() const {
  if (content_ == nullptr) {
    return 0U;
  }
  auto payload = content_->payload();
  assert(payload != nullptr);
  return payload->size();
}

std::optional<Timestamp> FileStore::EntryBase::originalPayloadEntryTimestamp() const {
  if (content_ == nullptr) {
    return std::nullopt;
  }
  return content_->getOriginalPayloadEntryTimestamp();
}

Generator<std::string> FileStore::EntryBase::getPageNames() const {
  if (content_ == nullptr) {
    co_return;
  }
  auto payload = content_->payload();
  assert(payload != nullptr);
  for (auto&& path : payload->getPageNames()) {
    co_yield std::move(path);
  }
}

Generator<std::string> FileStore::Entry::pageNames() const {
  if (this->getRedactedAt()) {
    co_return;
  }
  for (auto&& name : this->getPageNames()) {
    co_yield name;
  }
}

messaging::MessageSequence FileStore::Entry::readPage(size_t index) {
  const auto& content = this->content();
  if (content == nullptr) {
    throw std::runtime_error("Can't read page from tombstone");
  }
  auto payload = content->payload();
  assert(payload != nullptr);
  return payload->readPage(this->getFileStore().pagestore_, this->getCell().entryName(), index);
}

void FileStore::Entry::markRedacted(Timestamp at) noexcept {
  assert(!redactedAt_.has_value() && "Entry is already redacted");
  redactedAt_ = at;
}

void FileStore::Entry::throwErrorIfRedacted(const std::string& logContext) const {
  if (redactedAt_.has_value()) {
    auto name = this->getName();
    PEP_LOG(LogTag, Severity::Warning)
      << logContext << " can't process "
      << " entry redacted at " << TicksSinceEpoch<std::chrono::milliseconds>(*redactedAt_)
      << " (i.e. " << TimestampToXmlDateTime(*redactedAt_) << ')'
      << ": participant " << name.participant()
      << ", column " << name.column()
      << ", version " << TicksSinceEpoch<std::chrono::milliseconds>(validFrom_)
      << " (i.e. " << TimestampToXmlDateTime(validFrom_) << ')';
    throw Error("Attempt to access redacted entry");
  }
}

void FileStore::Entry::save() const {
  std::ostringstream out;

  out << EntryFileType;
  WriteBinary(out, this->getName().string());
  WriteBinary(out, static_cast<std::uint64_t>(TicksSinceEpoch<milliseconds>(validFrom_)));

  std::vector<PageId> pages;
  PersistedEntryProperties properties;
  SetPersistedEntryProperty(properties, ChecksumSubstituteKey, this->getChecksumSubstitute());
  if (auto redactedAt = this->getRedactedAt()) {
    SetPersistedEntryProperty(properties, RedactedAtKey, *redactedAt);
  }

  EntryContent::Save(this->content(), properties, pages);

  WriteBinary(out, pages);
  WriteBinary(out, properties);

  std::string content = std::move(out).str();
  XXH64_hash_t hash = XXH64(content.data(), content.length(), 0ULL);

  std::filesystem::path tempfile = this->getFilePath(".tmp");
  std::ofstream outfile;
  std::lock_guard lock(this->getCell().participant().fileStore().fileSystemMux_);
  outfile.open(tempfile, std::ios::binary | std::ios::out | std::ios::trunc);
  if (!outfile.is_open())
    throw std::invalid_argument("could not write file: " + tempfile.string());

  outfile << content;
  WriteBinary(outfile, uint64_t{ hash });
  if (!outfile) {
    throw std::runtime_error("failed to write content to file: " + tempfile.string());
  }
  outfile.close();

  std::filesystem::rename(tempfile, this->getFilePath(FileExtension));
}

void FileStore::EntryChange::commit(Timestamp availableFrom) && {
  if (!valid_)
    throw std::runtime_error(std::format("FileStore: change to entry already committed/cancelled: {}", this->getName()));
  if (availableFrom <= lastEntryValidFrom_)
    throw std::invalid_argument(std::format("FileStore: newer item is already available, can not store {}", this->getName()));
  auto newestEntry = this->getFileStore().lookup(this->getName());
  if (newestEntry && newestEntry->getValidFrom() > lastEntryValidFrom_)
    throw std::runtime_error(std::format("FileStore: concurrent modification to same entry detected: {}", this->getName()));
#if PEP_BUILD_HAS_DEBUG_FLAVOR()
  // this should not happen due to combination of above conditions:
  // - check that the availableFrom > last item (on time of modify() method)
  // - check that last item on time of modify() is still the last item at time of commit()
  if (this->getCell().versions().contains(availableFrom)) {
    auto msg = std::format("Cannot store duplicate entry with name {} and timestamp {}",
      this->getName(), TicksSinceEpoch<milliseconds>(availableFrom));
    PEP_LOG(LogTag, Severity::Error) << msg;
    throw std::runtime_error(msg);
  }
#endif

  const auto& content = this->content();
  if (content != nullptr && content->payload() == nullptr) { // this entry is not a tombstone but EntryChange::appendPage was never invoked, so...
    // ... this entry has empty payload, which we'll represent as a PagedEntryPayload without pages
    content->setPayload(std::make_shared<PagedEntryPayload>());
  }

  // Create memory data structure
  auto entry = Entry::Create(std::move(*this), availableFrom);
  // Prevent this EntryChange from being re-used (a.o. because we just std::moved it)
  valid_ = false;

  // Save to disk
  entry->save();
  // Include memory data structure in tree
  this->getCell().addEntry(entry);
}

void FileStore::EntryChange::cancel() && {
  if (!valid_)
    throw std::invalid_argument(std::format("FileStore: change to entry already committed/cancelled: {}", this->getName()));

  // FIXME: remove new pages

  valid_ = false;
}

std::shared_ptr<FileStore::Entry> FileStore::Entry::Load(Cell& cell, Timestamp timestamp) {
  // Note: On case-insensitive filesystems (e.g. Windows), file names could collide.
  // Additionally, NTFS 8.3 short names could collide (a column called "PARTIC~1" will collide with "ParticipantIdentifier").
  // See https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file.
  // This may lead to security issues.
  // So basically, we should not run the production StorageFacility on Windows with the current code.
  auto timestampStr = std::to_string(TicksSinceEpoch<milliseconds>(timestamp));
  auto result = TryLoad(cell, CheckedFileName(timestampStr + FileExtension));
  if (result == nullptr) {
    throw std::runtime_error(std::format("Could not load entry for cell {} at timestamp {}",
      cell.entryName().string(), timestampStr));
  }
  return result;
}

bool FileStore::Entry::EnsureRedactionPersisted(Timestamp redactedAt, Cell& cell, Timestamp version) {
  auto timestampStr = std::to_string(TicksSinceEpoch<milliseconds>(version));

  std::lock_guard lock(cell.participant().fileStore().fileSystemMux_);
  // Load the ".entry" file
  auto entry = TryLoad(cell, CheckedFileName(timestampStr + FileExtension));
  if (entry == nullptr) {
    std::ostringstream stream;
    stream << "Redaction persistence can't load entry file for cell " << cell.entryName().string() << " version " << timestampStr;
    auto msg = std::move(stream).str();
    PEP_LOG(LogTag, Severity::Error) << msg;
    throw std::runtime_error(msg);
  }

  if (entry->getRedactedAt().has_value()) {
    PEP_LOG(LogTag, Severity::Debug) << "Redaction has previously been persisted for cell " << cell.entryName().string() << " version " << timestampStr;
    assert(entry->getRedactedAt() == redactedAt);
    return false;
  }

  PEP_LOG(LogTag, Severity::Debug) << "Persisting redaction for cell " << cell.entryName().string() << " version " << timestampStr;
  entry->markRedacted(redactedAt);
  entry->save();
  PEP_LOG(LogTag, Severity::Info) << "Persisted redaction for cell " << cell.entryName().string() << " version " << timestampStr;
  return true;
}

std::shared_ptr<FileStore::Entry> FileStore::Entry::TryLoad(Cell& cell, const CheckedFileName& filename) {
  if (filename.path().extension() != FileExtension) { // Caller has found a non-entry file and attempts to load it
    return nullptr;
  }

  auto path = (cell.path() / filename).path();
  if (!std::filesystem::is_regular_file(path)) { // This is not one of our files
    return nullptr;
  }

  Timestamp validFrom(milliseconds{boost::lexical_cast<milliseconds::rep>(path.stem().string())});

  std::ifstream infile;
  infile.open(path, std::ios::binary | std::ios::in);
  if (!infile.is_open())
    throw std::invalid_argument("could not open file for reading");

  // Read magic bytes from start of file, validating that this is file indeed represents a file store entry
  std::string fileType(EntryFileType.size(), '\0');
  infile.read(fileType.data(), static_cast<std::streamsize>(fileType.size()));
  if (fileType != EntryFileType) {
    throw std::invalid_argument("could not read file (wrong file type): " + path.string());
  }

  // Read the entry's contents from the file
  auto storedName = ReadBinary(infile, std::string());
  if (storedName != cell.entryName().string()) {
    throw std::runtime_error("could not read file (wrong entry name)");
  }

  Timestamp storedValidFrom(milliseconds{ReadBinary(infile, std::uint64_t{})});
  if (storedValidFrom != validFrom) {
    throw std::runtime_error("could not read file (wrong validity timestamp)");
  }

  auto pages = ReadBinary(infile, std::vector<PageId>());
  auto properties = ReadBinary(infile, PersistedEntryProperties());

  auto checksumSubstitute = ExtractPersistedEntryProperty<uint64_t>(properties, ChecksumSubstituteKey);
  auto redactedAt = TryExtractPersistedEntryProperty<Timestamp>(properties, RedactedAtKey);
  auto entryContent = EntryContent::Load(cell.participant().fileStore(), properties, pages);

  // Read content hash from (end of) file
  uint64_t expectedHash = 0;
  expectedHash = ReadBinary(infile, expectedHash);
  if (!infile.good())
    throw std::invalid_argument("could not read file (error reading): " + path.string());

  // Read the data that the hash was calculated over: the entire file minus the hash itself
  auto size = static_cast<std::streamsize>(infile.tellg()) - static_cast<std::streamsize>(sizeof(expectedHash));
  std::string content(static_cast<size_t>(size), '\0');
  infile.seekg(0, std::ios::beg);
  infile.read(content.data(), size);
  if (!infile.good())
    throw std::invalid_argument("could not read file (for hash)");

  infile.close();

  // Validate file content against its hash
  uint64_t calculatedHash = XXH64(content.data(), content.length(), uint64_t{0});
  if (calculatedHash != expectedHash)
    throw std::invalid_argument("hash did not match for file " + path.string());

  return Entry::Create(cell, validFrom, redactedAt, checksumSubstitute, std::move(entryContent));
}

FileStore::Entry::Entry(EntryChange&& source, Timestamp validFrom)
  : EntryBase(std::move(source)), validFrom_(validFrom) {
  assert(this->content() == nullptr || this->content()->payload() != nullptr);
}

FileStore::Entry::Entry(Cell& cell, Timestamp validFrom, std::optional<Timestamp> redactedAt, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content)
  : EntryBase(cell, checksumSubstitute, std::move(content)), validFrom_(validFrom), redactedAt_(redactedAt) {
}

rxcpp::observable<std::string> FileStore::EntryChange::appendPage(std::shared_ptr<std::string> rawPage, uint64_t payloadSize, uint64_t pagenr) {
  if (pagenr == 0) {
    assert(pagedPayload_ == nullptr);
    assert(this->content()->payload() == nullptr);

    if (rawPage->size() < InlinePageThreshold) {
      auto payload = std::make_shared<InlinedEntryPayload>(*rawPage, payloadSize);
      this->content()->setPayload(payload);
      return rxcpp::observable<>::just(payload->getEtag());
    }

    pagedPayload_ = std::make_shared<PagedEntryPayload>();
    this->content()->setPayload(pagedPayload_);
  }

  if (pagedPayload_ == nullptr) {
    throw std::runtime_error("Can't append page to nonpaged payload");
  }
  return pagedPayload_->appendPage(*this->getFileStore().pagestore_, this->getName(), pagenr, rawPage, payloadSize);
}

EntryName FileStore::Cell::entryName() const {
  return EntryName(this->participant().name(), columnName_);
}

CheckedPath FileStore::Cell::path() const {
  return this->participant().path() / CheckedFileName(columnName_);
}

FileStore::Metrics FileStore::Cell::getMetrics() const {
  Metrics result{
    .entryCount = versions_.size(),
    .totalBytes = 0U,
    .redactedBytes = 0U,
    .rollingBytes = latest_ ? latest_->payloadSize() : 0U,
  };

  for (const CellVersion& version : versions_) {
    if (version.originalPayloadEntryTimestamp == std::nullopt) { // This version is the original payload entry
      result.totalBytes += version.payloadSize;
      if (version.redactedAt) {
        // Payload can only be discarded if no other version references it
        auto referenced = !(versions_ | std::views::filter([original = version.validFrom](const CellVersion& other) { return other.originalPayloadEntryTimestamp == original && !other.redactedAt; })).empty();
        if (!referenced) {
          result.redactedBytes += version.payloadSize;
        }
      }
    }
  }

  return result;
}

void FileStore::Cell::addEntry(std::shared_ptr<Entry> entry) {
  auto emplaced = versions_.emplace(CellVersion::FromEntry(*entry)).second;
  if (!emplaced) {
    auto msg = std::format("Couldn't overwrite existing entry with name {} and timestamp {}",
      entry->getName(), TicksSinceEpoch<milliseconds>(entry->getValidFrom()));
    PEP_LOG(LogTag, Severity::Error) << msg;
    throw std::runtime_error(msg);
  }
  if (latest_ == nullptr || entry->getValidFrom() > latest_->getValidFrom()) {
    latest_ = entry;
  }
}

FileStore::Cell* FileStore::Participant::getCell(const std::string& columnName) const {
  auto pos = cells_.find(columnName);
  if (pos == cells_.cend()) {
    return nullptr;
  }
  return &**pos;
}

FileStore::Cell& FileStore::Participant::provideCell(const std::string& columnName) {
  auto existing = this->getCell(columnName);
  if (existing != nullptr) {
    return *existing;
  }
  return **cells_.emplace(std::make_unique<Cell>(*this, columnName)).first;
}

PropertyBasedContainer<const FileStore::Cell*, &FileStore::Cell::columnName>::set FileStore::Participant::cells() const {
  PropertyBasedContainer<const FileStore::Cell*, &FileStore::Cell::columnName>::set result;
  for (const auto& cell : cells_) {
    [[maybe_unused]] auto emplaced = result.emplace(cell.get());
    assert(emplaced.second);
    assert(std::next(emplaced.first) == result.end()); // Should have been inserted at the back of the result set
  }
  return result;
}

CheckedPath FileStore::Participant::path() const {
  return this->fileStore().metaDir() / CheckedFileName(name_);
}

FileStore::Metrics FileStore::Participant::getMetrics(const std::set<std::string>& columns) const {
  FileStore::Metrics result;

  for (const auto& cell : cells_) {
    if (columns.empty() || columns.contains(cell->columnName())) {
      result += cell->getMetrics();
    }
  }

  return result;
}

std::shared_ptr<FileStore::Entry> FileStore::Participant::lookup(const std::string& column, Timestamp validAt) {
  auto cell = this->getCell(column);
  if (cell == nullptr) {
    return nullptr;
  }
  return cell->lookup(validAt);
}

Generator<PrefixedStringsEntry> FileStore::Participant::pagePaths() const {
  static_assert(DerivedFromSpecialization<decltype(cells_), std::set>, "Iterators need to remain valid when cells are added");
  for (const auto& cell : cells_) {
    co_yield PushPrefix{.prefix = std::format("{}{}", cell->columnName(), EntryName::Delimiter)};
    for (auto&& entry : cell->pagePaths()) {
      co_yield std::move(entry);
    }
    co_yield PopPrefix{};
  }
}

void FileStore::Participant::markRedacted(Timestamp redactedAt, const VersionTimestampsByColumn& vsByColumn) noexcept {
  for (const auto& c : vsByColumn) {
    auto& [columnName, versions] = c;
    auto cell = this->getCell(columnName);
    assert(cell != nullptr);
    cell->markRedacted(redactedAt, versions);
  }
}

FileStore::EntryRedactionPersister FileStore::Participant::makeRedactionPersister(Timestamp redactAt, const std::string& column, Timestamp version) const {
  auto cell = this->getCell(column);
  assert(cell != nullptr);
  return EntryRedactionPersister(redactAt, *cell, version);
}

}
