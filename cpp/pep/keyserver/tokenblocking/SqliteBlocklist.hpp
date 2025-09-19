#pragma once

#include <filesystem>
#include <memory>
#include <pep/keyserver/tokenblocking/Blocklist.hpp>

namespace pep::tokenBlocking {

/// A blocklist where all entries are stored in a sqlite database.
class SqliteBlocklist : public Blocklist {
public:
  ~SqliteBlocklist() override;

  /// Creates a non-persistent SqlBlocklist, where the underlying data is stored in memory.
  static std::unique_ptr<SqliteBlocklist> CreateWithMemoryStorage();

  /// @brief Creates a persistent SqlBlocklist, where the underlying data is stored on disk.
  ///
  /// If @p dbFile is a valid path then the data is synced to a sqlite database on that location.
  /// Will create a new database if it does not exist yet on the given location.
  ///
  /// @throws std::runtime_error When @p dbFile is not a plain path, but something that has special meaning in Sqlite.
  ///         We reject these because otherwise we cannot guarantee correct behavior of the object.
  static std::unique_ptr<SqliteBlocklist> CreateWithStorageLocation(const std::filesystem::path& dbFile);

  std::size_t size() const override;
  std::vector<Entry> allEntries() const override;
  std::vector<Entry> allEntriesMatching(const TokenIdentifier&) const override;
  std::optional<Entry> entryById(int64_t) const override;
  int64_t add(const TokenIdentifier&, const Entry::Metadata&) override;
  std::optional<Entry> removeById(int64_t) override;

  /// Returns true if the internal data is stored on disk and false if data is stored in memory only.
  bool isPersistent() const;

private:
  explicit SqliteBlocklist(const std::filesystem::path&);

  // using the pimpl idiom, so that we do not have to expose implementation dependencies in the header.
  struct Data;
  std::unique_ptr<Data> mData;
};

} // namespace pep::tokenBlocking
