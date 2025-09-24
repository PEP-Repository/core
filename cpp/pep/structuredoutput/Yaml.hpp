#pragma once

#include <pep/structuredoutput/Common.hpp>

#include <pep/accessmanager/UserMessages.hpp>

namespace pep::structuredOutput::yaml {

/// Appends the YAML representation of the response object to the stream.
std::ostream& append(std::ostream&, const pep::UserQueryResponse&, DisplayConfig = {});

} // namespace pep::structuredOutput::yaml
