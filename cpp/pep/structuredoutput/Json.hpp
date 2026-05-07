#pragma once

#include <ostream>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Tree.hpp>
#include <pep/structuredoutput/Common.hpp>

namespace pep::structuredOutput::json {

struct Config final {
  WhitespaceFormat wsformat = WhitespaceFormat::TwoSpaces;
};

/// Appends a JSON representation of a tree to a stream
inline std::ostream& append(std::ostream& stream, const Tree& tree, const Config& config = {}) {
  int spaces = 0;
  switch (config.wsformat) {
    case WhitespaceFormat::Compact: spaces = -1; break;
    case WhitespaceFormat::TwoSpaces: spaces = 2; break;
    case WhitespaceFormat::FourSpaces: spaces = 4; break;
  }
  return stream << tree.raw_json().dump(spaces);
}

/// Appends a JSON representation of a table to a stream
inline std::ostream& append(std::ostream& stream, const Table& table, const Config& config = {}) { 
  return append(stream, TreeFromTable(table), config); 
}

} // namespace pep::structuredOutput::json
