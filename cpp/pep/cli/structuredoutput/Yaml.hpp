#pragma once

#include <pep/cli/structuredoutput/Common.hpp>

#include <pep/authserver/AsaMessages.hpp>

namespace structuredOutput::yaml {

/// Appends the YAML representation of the response object to the stream.
std::ostream& append(std::ostream&, const pep::AsaQueryResponse&, DisplayConfig = {});

} // namespace structuredOutput::yaml