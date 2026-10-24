#pragma once

#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/ticketing/TicketingMessages.hpp>
#include <pep/weblib/Emscripten_fwd.hpp>
#include <pep/weblib/EmscriptenValPtr.hpp>

#include <emscripten/val.h>

#include <rxcpp/rx-lite.hpp>

#include <compare>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Structures used in weblib API

namespace pep::weblib {

struct ListQuery {
  std::optional<std::vector<std::string>> subjectGroups;
  std::optional<std::vector<std::string>> subjects;
  std::optional<std::vector<std::string>> columnGroups;
  std::optional<std::vector<std::string>> columns;
  /// Access mode to request the ticket for: "read" (default) or "read-meta" to also list metadata-only cells
  std::optional<std::string> mode;

  [[nodiscard]] auto operator<=>(const ListQuery&) const = default;
};

/// Access modes of the enrolled User Group to a column group.
/// Includes implied ones, unless \c ListColumnsQuery::includeImplicitlyGranted is false
struct ColumnGroupAccessModes {
  bool read = false;
  bool readMeta = false;
  bool write = false;
  bool writeMeta = false;

  [[nodiscard]] auto operator<=>(const ColumnGroupAccessModes&) const = default;
};

struct ListColumnsQuery {
  /// Also report modes implied by others: "read-meta" by "read", "write" by "write-meta". Default true
  std::optional<bool> includeImplicitlyGranted;
  /// Only return column groups on which the enrolled User Group has all of the set modes. Default none: all groups
  ColumnGroupAccessModes requireModes;

  [[nodiscard]] auto operator<=>(const ListColumnsQuery&) const = default;
};

/// Used in the result of listColumns
struct ColumnGroup {
  std::string name;
  ColumnGroupAccessModes modes;
  std::vector<std::string> columns;

  [[nodiscard]] auto operator<=>(const ColumnGroup&) const = default;
};

struct EnrolledUser {
  std::string userGroup;
  std::string user;

  [[nodiscard]] auto operator<=>(const EnrolledUser&) const = default;
};

/// Access modes of the enrolled User Group to a subject group: \c enumerate to list cells, \c access to retrieve them
struct SubjectGroupAccessModes {
  bool access = false;
  bool enumerate = false;

  [[nodiscard]] auto operator<=>(const SubjectGroupAccessModes&) const = default;
};

/// Used in the result of listSubjectGroups
struct SubjectGroup {
  std::string name;
  SubjectGroupAccessModes modes;

  [[nodiscard]] auto operator<=>(const SubjectGroup&) const = default;
};

struct CellEntry {
  std::shared_ptr<EnumerateResult> inner;
  std::shared_ptr<SignedTicket2> ticket;

  [[nodiscard]] std::string id() const;
  [[nodiscard]] std::string subjectLocalPseudonym() const;
  [[nodiscard]] std::string subjectEncryptedOriginId() const;
  [[nodiscard]] const std::string& column() const;
  // double to prevent it being mapped to bigint, which is harder to work with
  [[nodiscard]] double fileSize() const;
  [[nodiscard]] std::unordered_map<std::string, std::optional<emscripten::val>> partialMetadataView() const;
};

struct CellData {
  const CellEntry* entry;
  EmscriptenValPtr contentReadableStream;

  CellData(const CellEntry* entry, emscripten::val contentReadableStream);

  [[nodiscard]] emscripten::val content() const;
};

struct StoreQuery {
  std::string subject;
  std::string column;
  emscripten::val reader;
  std::unordered_map<std::string, std::string> metadata;
};

struct StoreResult {
  std::string id;
};

struct ParticipantPersonalia {
  std::string firstName;
  std::string middleName;
  std::string lastName;
  std::string dateOfBirth;

  [[nodiscard]] auto operator<=>(const ParticipantPersonalia&) const = default;
};

/// Used in the result of listUsers, to list the groups of each User
struct UserGroupMembership {
  std::string userGroup;
  /// Milliseconds since the Unix epoch. double to prevent it being mapped to bigint
  std::optional<double> expiration;

  [[nodiscard]] auto operator<=>(const UserGroupMembership&) const = default;
};

/// Used in the result of listUsers
struct User {
  std::optional<std::string> displayId;
  std::optional<std::string> primaryId;
  std::vector<std::string> otherUids;
  std::vector<UserGroupMembership> groups;

  [[nodiscard]] auto operator<=>(const User&) const = default;
};

/// Used in the result of listUsers, and as input of createUserGroup and modifyUserGroup.
/// Named differently from pep::UserGroup to prevent ambiguity
struct UserGroupInfo {
  std::string name;
  /// double to prevent it being mapped to bigint
  std::optional<double> maxAuthValiditySeconds;

  [[nodiscard]] auto operator<=>(const UserGroupInfo&) const = default;
};

/// Used as the result of listUsers
struct UserQueryResult {
  std::vector<User> users;
  std::vector<UserGroupInfo> userGroups;

  [[nodiscard]] auto operator<=>(const UserQueryResult&) const = default;
};

}
