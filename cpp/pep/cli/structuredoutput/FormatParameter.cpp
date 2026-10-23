#include <pep/cli/structuredoutput/FormatParameter.hpp>

#include <pep/structuredoutput/Csv.hpp>
#include <pep/structuredoutput/Json.hpp>
#include <pep/structuredoutput/Yaml.hpp>
#include <pep/utils/VariantUtils.hpp>

#include <stdexcept>

namespace pep::structuredOutput {
namespace {

constexpr auto jsonCompact = "json-compact";

}

commandline::Parameter FormatParameter(FormatFlags allowed, const std::optional<std::string>& defaultFormat) {
  auto formats = ToIndividualStrings(allowed);
  if (HasFlags(allowed, FormatFlags::Json)) {
    formats.emplace_back(jsonCompact);
  }

  auto value = commandline::Value<std::string>().allow(formats);
  if (defaultFormat) {
    value = value.defaultsTo(*defaultFormat);
  }
  return commandline::Parameter(formatParameterName, "The format of the output.").value(value);
}

FormatConfig FormatConfigFrom(const commandline::NamedValues& values, const YamlConfig& yamlConfig) {
  const auto format = values.get<std::string>(formatParameterName);
  if (format == "yaml") {
    return yamlConfig;
  }
  if (format == "json") {
    return JsonConfig{};
  }
  if (format == jsonCompact) {
    return JsonConfig{.wsFormat = WhitespaceFormat::Compact};
  }
  throw std::runtime_error("Unsupported output format: " + format);
}

std::ostream& WriteStructured(std::ostream& stream, const Tree& tree, const FormatConfig& config) {
  return std::visit(
      variant_utils::Overloaded{
          [&](const YamlConfig& yaml) -> std::ostream& {
            return yaml::append(stream, tree, yaml) << std::flush; // yaml output already ends with a newline
          },
          [&](const JsonConfig& json) -> std::ostream& {
            return json::append(stream, tree, json) << std::endl;
          }},
      config);
}

std::ostream& WriteStructured(std::ostream& stream, const Table& table, const commandline::NamedValues& values) {
  if (values.get<std::string>(formatParameterName) == "csv") {
    return csv::append(stream, table) << std::endl;
  }
  return WriteStructured(stream, TreeFromTable(table), FormatConfigFrom(values));
}

} // namespace pep::structuredOutput
