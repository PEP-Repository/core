#pragma once

#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/structuredoutput/Tree.hpp>

namespace pep::structuredOutput::yaml {

struct Config final {
  /// Adds a comment to the header of each non-empty list, displaying the number of elements
  bool includeArraySizeComments = false;
};

/// Appends the YAML representation of the response object to the stream.
std::ostream& append(std::ostream&, const pep::UserQueryResponse&, DisplayConfig = {});

/// Appends a YAML representation of a tree to a stream
std::ostream& append(std::ostream& stream, const Tree& tree, Config = {});

/// Appends a YAML representation of a table to a stream
inline std::ostream& append(std::ostream& stream, const Table& table, Config config = {}) {
  return append(stream, TreeFromTable(table), config);
}

/// Converts a tree to string.
/// @details This is a small wrapper around append for convenience.
std::string to_string(const Tree&, Config = {});

/// Converts a table to string.
/// @details This is a small wrapper around append for convenience.
inline std::string to_string(const Table& table, Config config = {}) { return to_string(TreeFromTable(table), config); }

} // namespace pep::structuredOutput::yaml
