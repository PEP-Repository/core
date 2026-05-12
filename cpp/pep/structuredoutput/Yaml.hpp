#pragma once

#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/structuredoutput/Tree.hpp>

namespace pep::structuredOutput::yaml {

struct Config final {
  WhitespaceFormat indentation = WhitespaceFormat::TwoSpaces;
  bool includeArraySizeComments = true; ///< Adds a comment to the header of each non-empty list, displaying the number of elements
  std::size_t arrayCountCommentThreshold = 5; ///< Minimum array size to show item count comment (unless empty)
  bool includeEmptyArrayComments = false; ///< Show item count comment for empty arrays (size 0) for clarity
};

/// Appends a YAML representation of a tree to a stream
std::ostream& append(std::ostream& stream, const Tree& tree, const Config& = {});

/// Appends a YAML representation of a table to a stream
inline std::ostream& append(std::ostream& stream, const Table& table, const Config& config = {}) {
  return append(stream, TreeFromTable(table), config);
}

/// Converts a tree to string.
/// @details This is a small wrapper around append for convenience.
std::string to_string(const Tree&, const Config& = {});

/// Converts a table to string.
/// @details This is a small wrapper around append for convenience.
inline std::string to_string(const Table& table, const Config& config = {}) { return to_string(TreeFromTable(table), config); }

} // namespace pep::structuredOutput::yaml
