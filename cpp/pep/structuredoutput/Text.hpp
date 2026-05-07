#pragma once

#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/structuredoutput/Tree.hpp>

namespace pep::structuredOutput::text {

struct Config final {
  WhitespaceFormat indentation = WhitespaceFormat::TwoSpaces;
  bool includeElementCounts = false; ///< Adds element count to array headers, e.g., "Columns (59):"
};

/// Appends a text representation of a tree to a stream
std::ostream& append(std::ostream& stream, const Tree& tree, const Config& = {});

/// Appends a text representation of a table to a stream
inline std::ostream& append(std::ostream& stream, const Table& table, const Config& config = {}) {
  return append(stream, TreeFromTable(table), config);
}

} // namespace pep::structuredOutput::text
