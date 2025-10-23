#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <filesystem>
#include <pep/keyserver/tokenblocking/SqliteBlocklist.hpp>
#include <pep/database/Storage.hpp>
#include <type_traits>

namespace pep::tokenBlocking {
namespace {

using namespace sqlite_orm; // Heavily used throughout this file

struct FlatEntry final {
  int64_t id = 0;
  std::string targetSubject;
  std::string targetUserGroup;
  int64_t targetIssueDateTime = 0;
  std::string metadataNote;
  std::string metadataIssuer;
  int64_t metaCreationDateTime = 0;
};

/// Consumes a FlatEntry to build a Blocklist::Entry
Blocklist::Entry Inflate(FlatEntry&& flat) {
  return {
      .id = flat.id,
      .target =
          {.subject = std::move(flat.targetSubject),
           .userGroup = std::move(flat.targetUserGroup),
           .issueDateTime = Timestamp{flat.targetIssueDateTime}},
      .metadata = {
          .note = std::move(flat.metadataNote),
          .issuer = std::move(flat.metadataIssuer),
          .creationDateTime = Timestamp{flat.metaCreationDateTime}}};
}

/// Consumes an optional FlatEntry to build an optional Blocklist::Entry
std::optional<Blocklist::Entry> Inflate(std::optional<FlatEntry>&& flat) {
  return flat.has_value() ? std::make_optional(Inflate(std::move(*flat))) : std::nullopt;
}

/// Consumes a FlatEntry vector to build a Blocklist::Entry vector
std::vector<Blocklist::Entry> InflateAll(std::vector<FlatEntry>&& flat) {
  std::vector<Blocklist::Entry> result;
  result.reserve(flat.size());
  for (auto&& flatEntry : flat) { result.emplace_back(Inflate(std::move(flatEntry))); }
  return result;
}

auto MakeStorage(const std::filesystem::path& dbFile) {
  auto storage = make_storage(
      dbFile.string(),
      make_table(
          "blocklistEntries",
          make_column("id", &FlatEntry::id, primary_key()),
          make_column("targetSubject", &FlatEntry::targetSubject),
          make_column("targetUserGroup", &FlatEntry::targetUserGroup),
          make_column("targetIssueDateTime", &FlatEntry::targetIssueDateTime),
          make_column("metadataNote", &FlatEntry::metadataNote),
          make_column("metadataIssuer", &FlatEntry::metadataIssuer),
          make_column("creationDateTime", &FlatEntry::metaCreationDateTime)));
  return storage;
}

/// Returns true if the path has no special meaning within Sqlite.
bool IsPlainPath(const std::filesystem::path& path) {
  return !path.empty() && path != database::BasicStorage::STORE_IN_MEMORY
    && !boost::trim_left_copy(path.string()).starts_with("file:");
}

} // namespace

struct SqliteBlocklist::Data final : database::Storage<MakeStorage> {
  explicit Data(const std::filesystem::path& path)
    : Storage(path.string()) {
    assert(IsPlainPath(path) || !isPersistent); // Rule out other non-plain paths
    syncSchema();
  }
};

SqliteBlocklist::~SqliteBlocklist() = default;

SqliteBlocklist::SqliteBlocklist(const std::filesystem::path& dbFile) : mData{std::make_unique<Data>(dbFile)} {}

std::unique_ptr<SqliteBlocklist> SqliteBlocklist::CreateWithMemoryStorage() {
  // std::make_unique does not work here because the constructor is private
  return std::unique_ptr<SqliteBlocklist>{new SqliteBlocklist{Data::STORE_IN_MEMORY}};
}

std::unique_ptr<SqliteBlocklist> SqliteBlocklist::CreateWithStorageLocation(const std::filesystem::path& dbFile) {
  if (!IsPlainPath(dbFile)) {
    throw std::runtime_error{
        "Illegal Argument: received \"" + dbFile.string()
        + "\", where a plain path, that is not empty and has no special meaning within sqlite, was expected."};
  }

  // std::make_unique does not work here because the constructor is private
  return std::unique_ptr<SqliteBlocklist>{new SqliteBlocklist{dbFile}};
}

std::size_t SqliteBlocklist::size() const { return static_cast<std::size_t>(mData->raw.count<FlatEntry>()); }

std::vector<Blocklist::Entry> SqliteBlocklist::allEntries() const {
  auto retrievedEntries = mData->raw.get_all<FlatEntry>();
  return InflateAll(std::move(retrievedEntries));
}

std::vector<Blocklist::Entry> SqliteBlocklist::allEntriesMatching(const TokenIdentifier& t) const {
  return InflateAll(mData->raw.get_all<FlatEntry>(where(
      c(&FlatEntry::targetSubject) == t.subject and c(&FlatEntry::targetUserGroup) == t.userGroup
      and c(&FlatEntry::targetIssueDateTime) >= t.issueDateTime.getTime())));
}

std::optional<Blocklist::Entry> SqliteBlocklist::entryById(int64_t id) const {
  return Inflate(mData->raw.get_optional<FlatEntry>(id));
}

int64_t SqliteBlocklist::add(const TokenIdentifier& t, const Entry::Metadata& m) {
  return mData->raw.insert(FlatEntry{
      .id = 0, // overwritten in storage
      .targetSubject = t.subject,
      .targetUserGroup = t.userGroup,
      .targetIssueDateTime = t.issueDateTime.getTime(),
      .metadataNote = m.note,
      .metadataIssuer = m.issuer,
      .metaCreationDateTime = m.creationDateTime.getTime()});
}

std::optional<Blocklist::Entry> SqliteBlocklist::removeById(int64_t id) {
  std::optional<FlatEntry> removedEntry;
  mData->raw.transaction([&] {
    removedEntry = mData->raw.get_optional<FlatEntry>(id);
    mData->raw.remove<FlatEntry>(id);
    return true;
  });
  return Inflate(std::move(removedEntry));
}

bool SqliteBlocklist::isPersistent() const { return mData->isPersistent; }

} // namespace pep::tokenBlocking
