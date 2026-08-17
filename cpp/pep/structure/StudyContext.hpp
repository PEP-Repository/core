#pragma once

#include <pep/structure/ShortPseudonyms.hpp>

#include <string>
#include <vector>

namespace pep {

class StudyContexts;

/// A study hosted in this PEP environment, identified by an id such as "GUM" or "BUGS".
/// Contexts are defined in the GlobalConfiguration ("study_contexts"). Configuration
/// items (short pseudonym definitions, devices) specify the single context they apply
/// to in their "study_context" field, while the contexts a participant belongs to are
/// stored in their "StudyContexts" column as a comma-separated string of ids. One
/// context is the default: its id is left out of such values and of generated column
/// names. Ids are compared case-insensitively, but generated values (column names,
/// participant data) use the casing as configured.
class StudyContext {
  friend class StudyContexts;

private:
  std::string id_;
  bool isDefault_;

private:
  StudyContext(std::string id, bool isDefault) : id_(std::move(id)), isDefault_(isDefault) {}

public:
  /// \throws std::runtime_error if the id is empty or contains characters other than
  /// alphanumerics and underscores.
  explicit StudyContext(std::string id);

  bool isDefault() const noexcept { return isDefault_; }
  inline const std::string& getId() const noexcept { return id_; }
  /// The id, or empty string for the default context.
  std::string getIdIfNonDefault() const;

  /// Whether this context occurs in a comma-separated string of context ids
  /// (compared case-insensitively). An empty string denotes (only) the default context.
  bool matches(const std::string& contexts) const;
  /// Whether the short pseudonym definition applies to this context.
  bool matchesShortPseudonym(const pep::ShortPseudonymDefinition& sp) const;

  /// Name of the column storing which assessor administered the given visit,
  /// e.g. "Visit1.Assessor" (default context) or "BUGS.Visit1.Assessor".
  std::string getAdministeringAssessorColumnName(uint32_t visitNumber) const; // 1-based

  /// Compares ids case-insensitively.
  bool operator ==(const StudyContext& other) const;
  inline bool operator !=(const StudyContext& other) const { return !(*this == other); }
};

/// A collection of study contexts, e.g. all contexts defined in the GlobalConfiguration,
/// or the subset that a participant belongs to.
class StudyContexts {
private:
  std::vector<StudyContext> items_;

private:
  std::vector<StudyContext>::const_iterator getPositionOf(const StudyContext& context) const;
  std::vector<StudyContext>::const_iterator findById(const std::string& id) const;

public:
  StudyContexts() = default;
  /// The first item is marked as the default context. If no items are provided, a
  /// single default context with an empty id is created.
  /// \throws std::runtime_error if an item is already marked as default, or if
  /// multiple items have the same id (compared case-insensitively).
  explicit StudyContexts(std::vector<StudyContext> items);

  bool contains(const StudyContext& context) const;
  const std::vector<StudyContext>& getItems() const noexcept { return items_; }

  /// \throws std::runtime_error if a context with the same id already exists
  /// (compared case-insensitively).
  void add(const StudyContext& context);
  void remove(const StudyContext& context);

  /// Looks up a context by its id, compared case-insensitively.
  /// \throws std::runtime_error if no context with the given id exists.
  const StudyContext& getById(const std::string& id) const;
  /// The default context, or nullptr if there is none (empty collection).
  const StudyContext* getDefault() const noexcept;

  /// The comma-separated ids of these contexts, e.g. to store as participant data.
  std::string toString() const;
  /// Looks up the subset of these contexts denoted by a comma-separated string of ids,
  /// as produced by toString. An empty string yields (only) the default context.
  /// \throws std::runtime_error if an id is not found, or if the string is empty and
  /// there is no default context.
  StudyContexts parse(const std::string& value) const;
};

}
