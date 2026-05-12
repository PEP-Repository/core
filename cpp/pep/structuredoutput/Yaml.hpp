#pragma once

#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/structuredoutput/Tree.hpp>

namespace pep::structuredOutput::yaml {

/// Appends a YAML representation of a tree to a stream
std::ostream& append(std::ostream& stream, const Tree& tree, const YamlConfig& = {});

/// Appends a YAML representation of a table to a stream
inline std::ostream& append(std::ostream& stream, const Table& table, const YamlConfig& config = {}) {
  return append(stream, TreeFromTable(table), config);
}

/// Converts a tree to string.
/// @details This is a small wrapper around append for convenience.
std::string to_string(const Tree&, const YamlConfig& = {});

/// Converts a table to string.
/// @details This is a small wrapper around append for convenience.
inline std::string to_string(const Table& table, const YamlConfig& config = {}) { return to_string(TreeFromTable(table), config); }

} // namespace pep::structuredOutput::yaml
