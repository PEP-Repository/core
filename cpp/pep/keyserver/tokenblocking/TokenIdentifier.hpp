#pragma once

#include <pep/crypto/Timestamp.hpp>

#include <string>

namespace pep::tokenBlocking {

/// The data by which a token is identified.
struct TokenIdentifier final {
  std::string subject;     ///< Owner of a token.
  std::string userGroup;   ///< The user-group of a token.
  Timestamp issueDateTime; ///< The date and time at which a token was issued.

  bool operator==(const TokenIdentifier&) const = default;
  bool operator!=(const TokenIdentifier&) const = default;
};

/// Returns true iff rhs has the same subject and user-group as rhs and was issued at the same time or later.
/// @note A token supersedes itself under this definition.
inline bool Supersedes(const TokenIdentifier& lhs, const TokenIdentifier& rhs) {
  return lhs.subject == rhs.subject && lhs.userGroup == rhs.userGroup && lhs.issueDateTime >= rhs.issueDateTime;
}

} // namespace pep::tokenBlocking
