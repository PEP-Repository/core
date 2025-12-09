#pragma once

#include <pep/accessmanager/UserMessages.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/structuredoutput/Tree.hpp>

namespace pep::structuredOutput::yaml {

/// Appends the YAML representation of the response object to the stream.
std::ostream& append(std::ostream&, const pep::UserQueryResponse&, DisplayConfig = {});

/// Appends a YAML representation of a tree to a stream
std::ostream& append(std::ostream& stream, const Tree& tree);

/// Appends a YAML representation of a table to a stream
inline std::ostream& append(std::ostream& stream, const Table& table) { return append(stream, TreeFromTable(table)); }

/// Converts a tree to string.
/// @details This is a small wrapper around append for convenience.
std::string to_string(const Tree& tree);

} // namespace pep::structuredOutput::yaml
