#pragma once

#include <ostream>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Tree.hpp>
#include <pep/structuredoutput/Common.hpp>

namespace pep::structuredOutput::json {

constexpr int toIndent(WhitespaceFormat f) {
    switch (f) {
        case WhitespaceFormat::Compact: return -1;
        case WhitespaceFormat::TwoSpaces: return 2;
        case WhitespaceFormat::FourSpaces: return 4;
    }
    assert(false && "Unhandled WhitespaceFormat");
    return -1;
}

/// Appends a JSON representation of a tree to a stream
inline std::ostream& append(std::ostream& stream, const Tree& tree, const JsonConfig& config = {}) {
  return stream << tree.rawJson().dump(toIndent(config.wsFormat));
}

/// Appends a JSON representation of a table to a stream
inline std::ostream& append(std::ostream& stream, const Table& table, const JsonConfig& config = {}) { 
  return append(stream, TreeFromTable(table), config); 
}

} // namespace pep::structuredOutput::json
