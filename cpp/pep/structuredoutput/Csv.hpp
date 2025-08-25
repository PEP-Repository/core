#pragma once

#include <ostream>
#include <pep/structuredoutput/Table.hpp>

namespace pep::structuredOutput::csv {

/// Supported CSV delimiter characters
enum class Delimiter : char { comma = ',', semicolon = ';', tab = '\t' };

struct Config final {
  // With European regional settings on Windows, MS Excel expects semicolon delimiters by default, when importing CSV
  Delimiter delimiter = Delimiter::semicolon;  ///< The character used to delimit individual fields
};

/// Appends the CSV representation of the table object to the stream.
std::ostream& append(std::ostream&, const Table&, Config = {});

/// Converts a table to string.
/// @details This is a small wrapper around append for convenience.
std::string to_string(const Table&, Config = {});

} // namespace pep::structuredOutput::csv
