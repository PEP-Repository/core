#pragma once

#include <pep/cli/structuredoutput/Common.hpp>

#include <ostream>
#include <pep/authserver/AsaMessages.hpp>

namespace structuredOutput::json {

/// Appends the JSON representation of the response object to the stream.
std::ostream& append(std::ostream&, const pep::AsaQueryResponse&, DisplayConfig = {});

} // namespace structuredOutput::json