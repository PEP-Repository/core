#include <pep/storagefacility/FileStore.hpp>
#include <pep/storagefacility/EntryPayload.hpp>
#include <pep/utils/BuildFlavor.hpp>
#include <pep/storagefacility/Constants.hpp>
#include <pep/utils/Random.hpp>
#include <pep/utils/Raw.hpp>
#include <pep/morphing/MorphingSerializers.hpp>

#include <boost/lexical_cast.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace std::chrono;
using namespace std::literals;

namespace pep {

namespace {

const std::string CHECKSUM_SUBSTITUTE_KEY = "checksum-substitute";

uint64_t GenerateChecksumSubstitute() {
  uint64_t checksumSubstitute;
  RandomBytes(reinterpret_cast<uint8_t*>(&checksumSubstitute), sizeof(checksumSubstitute));
  return checksumSubstitute;
}

const std::string ENTRY_FILE_TYPE("pepentry");
const std::string LOG_TAG("StorageFacility");

}

/**
 * Design:
 * - metadata is stored on local file system
 * - pages is stored on 'data' volume (which can be migrated to the cloud later on rather easy)
 * - every stored item is xxhashed so it can be verified that no error occured
 * - on start all the metadata is loaded into memory (4 KiB per entry x 256k entries = 1 GiB of RAM)
 * - I/O model: all reads are from memory; writes will synchronous write to disk for consistency
 * - retrieving 40k items out of 360k items (no historical items) cost 92ms
 * - retrieving 2 latest items out of 40k historical items of a total of 360k items cost 99ms
 */

 /**
  * Challenges:
  * - correctly (with all error condition) retrieve data from S3 interface
  * - if there are many entries; starting will take longer (possible migrate to mmap()ed data structure)
  * - partitioning (within a host; but also mutliple storage facilities)
  */

const std::string FileStore::Entry::FILE_EXTENSION = ".entry";

EntryContent::MetadataEntry FileStore::makeMetadataEntry(std::string key, std::string value) {
  auto pos = mMetadataValues.emplace(std::move(key), std::set<std::string>()).first;
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
  auto pos = mParticipants.find(name);
  if (pos == mParticipants.cend()) {
    return nullptr;
  }
  return &**pos;
}

FileStore::Participant& FileStore::provideParticipant(const std::string& name) {
  auto existing = this->getParticipant(name);
  if (existing != nullptr) {
    return *existing;
  }

  return **mParticipants.emplace(std::make_unique<Participant>(*this, name)).first;
}

const std::string& FileStore::getColumnString(const std::string& value) {
  if (value.find(EntryName::DELIMITER) != std::string::npos) {
    throw std::runtime_error("Cell name may not contain an entry name delimiter");
  }
  return *mColumnNames.insert(value).first;
}

FileStore::FileStore(
  const std::filesystem::path& metadatapath,
  std::shared_ptr<Configuration> pageStoreConfig,
  std::shared_ptr<boost::asio::io_context> io_context,
  std::shared_ptr<prometheus::Registry> metrics_registry)
  : mPath(metadatapath),
  mPagestore(PageStore::Create(io_context, metrics_registry, pageStoreConfig))
{
  // throws when an error occurs while creating any of the given directories in the supplied path
  std::filesystem::create_directories(mPath);

  auto start_time = steady_clock::now();
  for (const auto& p : std::filesystem::directory_iterator(mPath)) {
    auto name = p.path().filename().string();
    if (std::filesystem::is_directory(p.path()) && name.size() == LocalPseudonym::TextLength()) {
      mParticipants.emplace(std::make_unique<Participant>(*this, name, true));
    }
  }

  duration<double> seconds(steady_clock::now() - start_time);
  std::stringstream message;
  message.setf(std::ios::fixed);
  message.precision(2);
  message << "Loaded " << this->entryCount() << " file store entries in " << seconds;
  if (seconds != decltype(seconds)::zero()) {
    message << " (" << (static_cast<double>(this->entryCount()) / seconds.count()) << " entries per second)";
  }
  LOG(LOG_TAG, info) << message.str();
}

FileStore::Participant::Participant(FileStore& store, std::string name, bool load)
  : mStore(store), mName(std::move(name)) {
  if (load) {
    for (const auto& p : std::filesystem::directory_iterator(this->path())) {
      if (std::filesystem::is_directory(p.path())) {
        mCells.emplace(std::make_unique<Cell>(*this, p.path().filename().string(), true));
      }
    }
  }
  else {
    // throws when an error occurs while creating any of the given directories in the supplied path
    std::filesystem::create_directories(this->path());
  }
}

FileStore::Cell::Cell(Participant& participant, const std::string& columnName, bool load)
  : mParticipant(participant), mColumnName(participant.getFileStore().getColumnString(columnName)) {
  if (load) {
    for (const auto& p : std::filesystem::directory_iterator(this->path())) {
      auto entry = Entry::TryLoad(*this, p.path());
      if (entry != nullptr) {
        this->addEntry(entry);
      }
    }
  }
  else {
    // throws when an error occurs while creating any of the given directories in the supplied path
    std::filesystem::create_directories(this->path());
  }
}

size_t FileStore::entryCount() const {
  size_t result = 0;
  for (const auto& participant : mParticipants) {
    result += participant->entryCount();
  }
  return result;
}

void FileStore::forEachEntryHeader(const std::function<void(const EntryHeader&)>& callback) const {
  /* This method must provide its entries to the callback in (backward compatible) lexicographic order, e.g.
   * 1. participant-a/column-x/timestamp-1
   * 2. participant-a/column-x/timestamp-2
   * 3. participant-a/column-y/timestamp-1
   * 4. participant-b/column-x/timestamp-1
   */
  for (const auto& participant : mParticipants) {
    participant->forEachEntryHeader(callback);
  }
}


FileStore::EntrySet FileStore::lookupWithHistory(const EntryName& name) const {
  auto participant = this->getParticipant(name.participant());
  if (participant == nullptr) {
    return FileStore::EntrySet();
  }
  return participant->lookupWithHistory(name.column());
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
  auto it = mEntryHeaders.lower_bound(find);

  // If we're positioned on the first item, the request was for a "validAt" before the first entry was stored.
  if (it == mEntryHeaders.begin())
    return nullptr;

  // Since we're positioned after the item we're interested in, we skip back.
  --it;

  assert(mLatest != nullptr);
  if (it->validFrom == mLatest->getValidFrom()) {
    return mLatest;
  }
  return Entry::Load(*this, it->validFrom);
}

std::shared_ptr<FileStore::EntryChange> FileStore::modifyEntry(const EntryName& name, bool createIfNeeded) {
  auto entry = lookup(name);
  if (entry) {
    return EntryChange::Create(*entry);
  }

  if (!createIfNeeded) {
    return nullptr;
  }

  return this->provideParticipant(name.participant()).createEntry(name.column());
}

FileStore::EntryChange::EntryChange(Cell& cell)
  : EntryBase(cell, GenerateChecksumSubstitute(), nullptr),
    mLastEntryValidFrom(Timestamp::zero()) {
}

FileStore::EntryChange::EntryChange(const Entry& overwrites)
  : EntryBase(overwrites.getCell(), GenerateChecksumSubstitute(), overwrites.cloneContent()), mLastEntryValidFrom(overwrites.getValidFrom()) {
}

std::filesystem::path FileStore::Entry::getFilePath(const std::string& extension) const {
  auto filename = std::to_string(this->getValidFrom().ticks_since_epoch<milliseconds>()) + extension;
  return this->getCell().path() / filename;
}

FileStore::EntryBase::EntryBase(Cell& cell, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content)
  : mCell(cell), mChecksumSubstitute(checksumSubstitute), mContent(std::move(content)) {
}

std::unique_ptr<EntryContent> FileStore::Entry::cloneContent() const {
  const auto& content = this->content();
  if (content == nullptr) {
    return nullptr;
  }
  return std::make_unique<EntryContent>(*content, mValidFrom);
}

FileStore& FileStore::EntryBase::getFileStore() const noexcept {
  return this->getCell().getParticipant().getFileStore();
}

EntryName FileStore::EntryBase::getName() const {
  return this->getCell().entryName();
}

FileStore::EntryHeader FileStore::Entry::header() const {
  return EntryHeader{ this->getValidFrom(), this->getChecksumSubstitute() };
}

messaging::MessageSequence FileStore::Entry::readPage(size_t index) {
  const auto& content = this->content();
  if (content == nullptr) {
    throw std::runtime_error("Can't read page from tombstone");
  }
  auto payload = content->payload();
  assert(payload != nullptr);
  const auto& cell = this->getCell();
  return payload->readPage(cell.getParticipant().getFileStore().mPagestore, cell.entryName(), index);
}

void FileStore::Entry::save() const {
  std::stringstream out;

  out << ENTRY_FILE_TYPE;
  WriteBinary(out, this->getName().string());
  WriteBinary(out, static_cast<std::uint64_t>(mValidFrom.ticks_since_epoch<milliseconds>()));

  std::vector<PageId> pages;
  PersistedEntryProperties properties;
  SetPersistedEntryProperty(properties, CHECKSUM_SUBSTITUTE_KEY, this->getChecksumSubstitute());

  EntryContent::Save(this->content(), properties, pages);

  WriteBinary(out, pages);
  WriteBinary(out, properties);

  std::string content = out.str();
  XXH64_hash_t hash = XXH64(content.data(), content.length(), 0ULL);

  auto tempfile = this->getFilePath(".tmp");
  std::ofstream outfile;
  outfile.open(tempfile.string(), std::ios::binary | std::ios::out | std::ios::trunc);
  if (!outfile.is_open())
    throw std::invalid_argument("could not write file: " + tempfile.string());

  outfile << content;
  WriteBinary(outfile, uint64_t{hash});
  if (!outfile) {
    throw std::runtime_error("failed to write content to file: " + tempfile.string());
  }
  outfile.close();

  std::filesystem::rename(tempfile, this->getFilePath(Entry::FILE_EXTENSION));
}

void FileStore::EntryChange::commit(Timestamp availableFrom) && {
  if (!mValid)
    throw std::runtime_error("FileStore: change to entry already committed/cancelled: " + this->getName().string());
  if (availableFrom <= mLastEntryValidFrom)
    throw std::invalid_argument("FileStore: newer item is already available, can not store " + this->getName().string());
  auto newestEntry = this->getFileStore().lookup(this->getName());
  if (newestEntry && newestEntry->getValidFrom() > mLastEntryValidFrom)
    throw std::runtime_error("FileStore: concurrent modification to same entry detected: " + this->getName().string());
#if BUILD_HAS_DEBUG_FLAVOR()
  // this should not happen due to combination of above conditions:
  // - check that the availableFrom > last item (on time of modify() method)
  // - check that last item on time of modify() is still the last item at time of commit()
  if (this->getCell().entryHeaders().find(availableFrom) != this->getCell().entryHeaders().cend()) {
    auto msg = "Cannot store duplicate entry with name " + this->getName().string()
        + " and timestamp " + std::to_string(availableFrom.ticks_since_epoch<milliseconds>());
    LOG(LOG_TAG, error) << msg;
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
  mValid = false;

  // Save to disk
  entry->save();
  // Include memory data structure in tree
  this->getCell().addEntry(entry);
}

void FileStore::EntryChange::cancel() && {
  if (!mValid)
    throw std::invalid_argument("FileStore: change to entry already committed/cancelled: " + this->getName().string());

  // FIXME: remove new pages

  mValid = false;
}

std::shared_ptr<FileStore::Entry> FileStore::Entry::Load(Cell& cell, Timestamp timestamp) {
  auto filename = std::to_string(timestamp.ticks_since_epoch<milliseconds>()) + FILE_EXTENSION;
  auto result = TryLoad(cell, cell.path() / filename);
  if (result == nullptr) {
    throw std::runtime_error("Could not load entry for cell " + cell.entryName().string()
        + " at timestamp " + std::to_string(timestamp.ticks_since_epoch<milliseconds>()));
  }
  return result;
}

std::shared_ptr<FileStore::Entry> FileStore::Entry::TryLoad(Cell& cell, const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path) || path.extension().string() != FILE_EXTENSION) {
    return nullptr;
  }

  auto name = path.filename().string();
  name = name.substr(0, name.size() - FILE_EXTENSION.size());
  Timestamp validFrom(milliseconds{boost::lexical_cast<milliseconds::rep>(name)});

  std::ifstream infile;
  infile.open(path.string(), std::ios::binary | std::ios::in);
  if (!infile.is_open())
    throw std::invalid_argument("could not open file for reading");

  // Read magic bytes from start of file, validating that this is file indeed represents a file store entry
  std::string fileType(ENTRY_FILE_TYPE.size(), '\0');
  infile.read(fileType.data(), static_cast<std::streamsize>(fileType.size()));
  if (fileType != ENTRY_FILE_TYPE) {
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

  auto checksumSubstitute = ExtractPersistedEntryProperty<uint64_t>(properties, CHECKSUM_SUBSTITUTE_KEY);
  auto entryContent = EntryContent::Load(cell.getParticipant().getFileStore(), properties, pages);

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

  return Entry::Create(cell, validFrom, checksumSubstitute, std::move(entryContent));
}

FileStore::Entry::Entry(EntryChange&& source, Timestamp validFrom)
  : EntryBase(std::move(source)), mValidFrom(validFrom) {
  assert(this->content() == nullptr || this->content()->payload() != nullptr);
}

FileStore::Entry::Entry(Cell& cell, Timestamp validFrom, uint64_t checksumSubstitute, std::unique_ptr<EntryContent> content)
  : EntryBase(cell, checksumSubstitute, std::move(content)), mValidFrom(validFrom) {
}

rxcpp::observable<std::string> FileStore::EntryChange::appendPage(std::shared_ptr<std::string> rawPage, uint64_t payloadSize, uint64_t pagenr) {
  if (pagenr == 0) {
    assert(mPagedPayload == nullptr);
    assert(this->content()->payload() == nullptr);

    if (rawPage->size() < INLINE_PAGE_THRESHOLD) {
      auto payload = std::make_shared<InlinedEntryPayload>(*rawPage, payloadSize);
      this->content()->setPayload(payload);
      return rxcpp::observable<>::just(payload->getEtag());
    }

    mPagedPayload = std::make_shared<PagedEntryPayload>();
    this->content()->setPayload(mPagedPayload);
  }

  if (mPagedPayload == nullptr) {
    throw std::runtime_error("Can't append page to nonpaged payload");
  }
  return mPagedPayload->appendPage(*this->getFileStore().mPagestore, this->getName(), pagenr, rawPage, payloadSize);
}

EntryName FileStore::Cell::entryName() const {
  return EntryName(this->getParticipant().name(), mColumnName);
}

std::filesystem::path FileStore::Cell::path() const {
  return this->getParticipant().path() / mColumnName;
}

void FileStore::Cell::addEntry(std::shared_ptr<Entry> entry) {
  auto emplaced = mEntryHeaders.emplace(entry->header()).second;
  if (!emplaced) {
    auto msg = "Couldn't overwrite existing entry with name " + entry->getName().string()
        + " and timestamp " + std::to_string(entry->getValidFrom().ticks_since_epoch<milliseconds>());
    LOG(LOG_TAG, error) << msg;
    throw std::runtime_error(msg);
  }
  if (mLatest == nullptr || entry->getValidFrom() > mLatest->getValidFrom()) {
    mLatest = entry;
  }
}

FileStore::Cell* FileStore::Participant::getCell(const std::string& columnName) const {
  auto pos = mCells.find(columnName);
  if (pos == mCells.cend()) {
    return nullptr;
  }
  return &**pos;
}

FileStore::Cell& FileStore::Participant::provideCell(const std::string& columnName) {
  auto existing = this->getCell(columnName);
  if (existing != nullptr) {
    return *existing;
  }
  return **mCells.emplace(std::make_unique<Cell>(*this, columnName)).first;
}

std::filesystem::path FileStore::Participant::path() const {
  return this->getFileStore().metaDir() / mName;
}

size_t FileStore::Participant::entryCount() const {
  size_t result = 0U;
  for (const auto& cell : mCells) {
    result += cell->entryHeaders().size();
  }
  return result;
}

void FileStore::Participant::forEachEntryHeader(const std::function<void(const EntryHeader&)>& callback) const {
  for (const auto& cell : mCells) {
    for (const auto& header : cell->entryHeaders()) {
      callback(header);
    }
  }
}

FileStore::EntrySet FileStore::Participant::lookupWithHistory(const std::string& column) const {
  FileStore::EntrySet result;
  auto cell = this->getCell(column);
  if (cell != nullptr) {
    for (const auto& header : cell->entryHeaders()) {
      auto entry = Entry::Load(*cell, header.validFrom);
      [[maybe_unused]] auto emplaced = result.emplace(entry).second;
      assert(emplaced);
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

}
