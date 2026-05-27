#include <pep/structuredoutput/Yaml.hpp>

#include <pep/utils/ChronoUtil.hpp>

namespace pep::structuredOutput::yaml {
namespace {

enum class ForceQuotes { No, Yes };

std::ostream& AppendStringLiteral(std::ostream& stream, const std::string_view str, ForceQuotes forceQuotes) {
  constexpr auto needsQuotes = [](std::string_view str) {
    // applying quotes generously even though YAML would allow more to go without quotes
    return str.empty() ||
      !std::isalpha(str.front()) ||
      !std::all_of(str.begin(), str.end(), [](char c) { return std::isalnum(c) || c == '_' || c == ' '; });
  };
  constexpr auto needsEscape = [](char c) { return c == '\\' || c == '"'; };

  const auto quoteOrNothing = (forceQuotes == ForceQuotes::Yes || needsQuotes(str)) ? "\"" : "";

  stream << quoteOrNothing;
  for (char c : str) { stream << (needsEscape(c) ? "\\" : "") << c; }
  return stream << quoteOrNothing;
}

/// Recursive function to convert a JSON object to a YAML string.
/// @note does NOT prefix the output with indentation,
///       the caller should make sure that the output stream is at the correct initial indentation level
/// @note DOES append a newline character to the output
void SerializeJsonAsYaml(std::ostream& stream, const YamlConfig& config, nlohmann::ordered_json node, std::size_t indentLevel = {}) {
  const std::size_t spacesPerLevel = (config.indentation == WhitespaceFormat::FourSpaces) ? 4 : 2;
  const auto indent = std::string(spacesPerLevel * indentLevel, ' ');

  /// does nothing on the first call and appends indentation on subsequent calls
  auto indentIfNotFirst = [first = true, &indent](std::ostream& stream) mutable {
    if (!first) { stream << indent; }
    first = false;
  };

  const auto isAtomic = [](const nlohmann::ordered_json& node) {
    return !(node.is_object() || node.is_array()) || node.empty();
  };

  if (node.is_null()) { stream << "null\n"; }
  else if (node.is_number_integer()) { stream << std::to_string(node.get<int>()) + "\n"; }
  else if (node.is_number_float()) { stream << std::to_string(node.get<double>()) + "\n"; }
  else if (node.is_boolean()) { stream << (node.get<bool>() ? "true\n" : "false\n"); }
  else if (node.empty()) {
    stream << (node.is_array() ? "[]" : "{}");
    if (node.is_array() && config.includeArraySizeComments && config.includeEmptyArrayComments) {
      stream << " # item count: 0";
    }
    stream << "\n";
  }
  else if (node.is_string()) { AppendStringLiteral(stream, node.get<std::string>(), ForceQuotes::Yes) << "\n"; }
  else if (node.is_object()) {
    for (auto it = node.begin(); it != node.end(); it++) {
      indentIfNotFirst(stream);
      AppendStringLiteral(stream, it.key(), ForceQuotes::No) << ":";

      if (isAtomic(*it)) { 
        stream << " "; 
      }
      else {
        if (it->is_array() && config.includeArraySizeComments && 
            ((it->size() == 0 && config.includeEmptyArrayComments) || it->size() >= config.arrayCountCommentThreshold)) {
          stream << " # item count: " << it->size();
        }
        stream << '\n' << indent << std::string(spacesPerLevel, ' ');
      }
      SerializeJsonAsYaml(stream, config, it.value(), indentLevel + 1);
    }
  }
  else if (node.is_array()) {
    for (const auto& element : node) {
      indentIfNotFirst(stream);
      stream << "- ";
      if (element.is_array() && !element.empty()) {
        if (config.includeArraySizeComments && element.size() >= config.arrayCountCommentThreshold) {
          stream << "# item count: " << element.size();
        }
        stream << "\n" << indent << std::string(spacesPerLevel, ' ');
      }
      SerializeJsonAsYaml(stream, config, element, indentLevel + 1);
    }
  }
}

} // namespace

/// Appends a YAML representation of a tree to a stream
std::ostream& append(std::ostream& stream, const Tree& tree, const YamlConfig& config) {
  SerializeJsonAsYaml(stream, config, tree.rawJson());
  return stream;
}

/// Converts a tree to string.
/// @details This is a small wrapper around append for convenience.
std::string to_string(const Tree& tree, const YamlConfig& config) {
  std::ostringstream stream;
  append(stream, tree, config);
  return std::move(stream).str();
}

} // namespace pep::structuredOutput::yaml
