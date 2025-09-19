#pragma once

#include <ostream>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Tree.hpp>
#include <pep/structuredoutput/Common.hpp>

namespace pep::structuredOutput::json {

/// Appends a JSON representation of a tree to a stream
inline std::ostream& append(std::ostream& stream, const Tree& tree) {
  constexpr auto indentationSize = 2;
  return stream << tree.toJson().dump(indentationSize);
}

/// Appends a JSON representation of a table to a stream
inline std::ostream& append(std::ostream& stream, const Table& table) { return append(stream, TreeFromTable(table)); }

/// Appends a JSON representation of a response object to a stream.
std::ostream& append(std::ostream&, const pep::UserQueryResponse&, DisplayConfig = {});

} // namespace pep::structuredOutput::json
