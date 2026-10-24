#pragma once

#include <pep/application/CommandLineParameter.hpp>
#include <pep/structuredoutput/Common.hpp>
#include <pep/structuredoutput/FormatFlags.hpp>
#include <pep/structuredoutput/Tree.hpp>

#include <optional>
#include <ostream>
#include <string>

namespace pep::structuredOutput {

/// Name of the command line parameter that selects the output format
constexpr auto formatParameterName = "format";

/// Creates the "--format" parameter that selects how a command prints its output.
/// \param allowed The formats that the command supports. "json-compact" is offered whenever JSON is allowed.
/// \param defaultFormat The format to use if the user doesn't specify one, or std::nullopt for no default.
commandline::Parameter FormatParameter(
    FormatFlags allowed = FormatFlags::Json | FormatFlags::Yaml,
    const std::optional<std::string>& defaultFormat = "yaml");

/// Converts the value of the "--format" parameter (yaml|json|json-compact) to a format config
/// \param yamlConfig The config to return when YAML was selected
FormatConfig FormatConfigFrom(const commandline::NamedValues& values, const YamlConfig& yamlConfig = {});

/// Writes a tree in the specified format, ending with a newline
std::ostream& WriteStructured(std::ostream& stream, const Tree& tree, const FormatConfig& config);

} // namespace pep::structuredOutput
