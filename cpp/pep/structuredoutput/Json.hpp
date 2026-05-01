#pragma once

#include <ostream>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Tree.hpp>
#include <pep/structuredoutput/Common.hpp>

namespace pep::structuredOutput::json {

struct Config final {
  int indent = 2; ///< Indentation size for JSON output
};

/// Appends a JSON representation of a tree to a stream
inline std::ostream& append(std::ostream& stream, const Tree& tree, const Config& config = {}) {
  return stream << tree.toJson().dump(config.indent);
}

/// Appends a JSON representation of a table to a stream
inline std::ostream& append(std::ostream& stream, const Table& table, const Config& config = {}) { 
  return append(stream, TreeFromTable(table), config); 
}

} // namespace pep::structuredOutput::json
