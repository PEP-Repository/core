#pragma once

#include <cstdint>
#include <optional>
#include <pep/keyserver/tokenblocking/BlocklistEntry.hpp>
#include <pep/keyserver/tokenblocking/TokenIdentifier.hpp>
#include <vector>

namespace pep::tokenBlocking {

/// A set of rules used to determine which tokens should be blocked.
class Blocklist {
public:
  using Entry = BlocklistEntry;

  Blocklist() = default;
  virtual ~Blocklist() = default;

  Blocklist(const Blocklist&) = delete;             // not copyable
  Blocklist& operator= (const Blocklist&) = delete; //

  Blocklist(Blocklist&&) = default;             // movable
  Blocklist& operator= (Blocklist&&) = default; //

  /// The number of entries.
  virtual std::size_t size() const = 0;

  /// Returns all entries in the list.
  virtual std::vector<Entry> allEntries() const = 0;

  /// Returns the entry for the given id if it exists or \c std::nullopt if no such entry exists.
  virtual std::optional<Entry> entryById(int64_t) const = 0;

  /// Returns all entries that have a matching target.
  virtual std::vector<Entry> allEntriesMatching(const TokenIdentifier&) const = 0;

  /// Adds a new entry and returns the id of that entry.
  virtual int64_t add(const TokenIdentifier&, const Entry::Metadata&) = 0;

  /// Removes an existing entry if it exists.
  /// @return The entry that was removed or \c std::nullopt if nothing was removed.
  virtual std::optional<Entry> removeById(int64_t) = 0;
};

/// Convenience function that returns true iff the @p token is matched by one or more entries on the @p list.
inline bool IsBlocking(const Blocklist& list, const TokenIdentifier& token) {
  return !list.allEntriesMatching(token).empty();
}

} // namespace pep::tokenBlocking
