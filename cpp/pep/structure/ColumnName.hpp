#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace pep {

/// One section of a period-delimited column name. When data is imported from Castor,
/// column names are generated as e.g. "<prefix>.<phase>.<step>" (CRF data) or
/// "<prefix>.<package>.<survey>.<step>" (survey data): the prefix is configured in the
/// GlobalConfiguration, while the other sections are named after Castor entities (see
/// castor::ImportColumnNamer).
class ColumnNameSection {
private:
  std::string value_;

public:
  explicit ColumnNameSection(std::string value);

  /// The mangled value, usable as a section in a column name.
  inline const std::string& getValue() const noexcept { return value_; }

  /// Mangles a raw external name into a valid column name section.
  static ColumnNameSection FromRawString(const std::string& raw);
};

/// Replacement of an (automatically generated) column name section by an alternative.
/// Castor names are long, descriptive and localized, making generated column names
/// cumbersome (downloaded files can even exceeded Windows path length limits), so Data
/// Administrator can define mappings to more readable sections.
struct ColumnNameMapping {
  ColumnNameSection original;
  ColumnNameSection mapped;
};

/// The set of ColumnNameMapping entries defined by Data Administrator, keyed by their
/// original (mangled) section value. Used during import to determine the column name
/// section to use for a Castor name.
class ColumnNameMappings {
private:
  std::unordered_map<std::string, ColumnNameMapping> entries_;

public:
  /// \throws std::runtime_error if multiple entries have the same original section.
  explicit ColumnNameMappings(const std::vector<ColumnNameMapping>& entries);

  std::vector<ColumnNameMapping> getEntries() const;

  // Returns the (DA) configured replacement for the (mangled) raw name, or the mangled
  // name itself if no mapping was configured for it.
  std::string getColumnNameSectionFor(const std::string& rawOriginal) const;
};

}
