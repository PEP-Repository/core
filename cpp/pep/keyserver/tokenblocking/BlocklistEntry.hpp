#pragma once

#include <pep/keyserver/tokenblocking/TokenIdentifier.hpp>

namespace pep::tokenBlocking {

/// Encodes a request to block a specific set of tokens.
struct BlocklistEntry final {
  struct Metadata final {
    std::string note;
    std::string issuer;
    Timestamp creationDateTime;

    bool operator== (const Metadata&) const = default;
    bool operator!= (const Metadata&) const = default;
  };

  int64_t id = 0;
  TokenIdentifier target;
  Metadata metadata;

  bool operator== (const BlocklistEntry&) const = default;
  bool operator!= (const BlocklistEntry&) const = default;
};

/// Returns true iff the target of the @p entry supersedes the @p token.
inline bool IsBlocking(const BlocklistEntry& entry, const TokenIdentifier& token) {
  return Supersedes(entry.target, token);
}

} // namespace pep::tokenBlocking
