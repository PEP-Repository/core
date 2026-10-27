#include <pep/structuredoutput/Yaml.hpp>

#include <pep/utils/ChronoUtil.hpp>

#include <format>
#include <locale>
#include <string_view>

namespace pep::structuredOutput::yaml {
namespace {

enum class ForceQuotes { No, Yes };

// TODO: also escape multi-byte UTF-8 characters that YAML wants escaped, which are now written as-is: C1 control
// characters (U+0080-U+009F) and the line and paragraph separators U+2028 and U+2029. Also handle invalid UTF-8.
// This requires decoding UTF-8, since this function only sees individual chars.
/// Appends a character to a double-quoted YAML string, escaping it if needed.
/// \remark Control characters are recognized in the "C" locale (std::locale::classic()), regardless of the global
///         locale, so only ASCII control characters are escaped.
void AppendEscaped(std::ostream& stream, char c) {
  switch (c) {
  case '\\': stream << "\\\\"; break;
  case '"': stream << "\\\""; break;
  // YAML turns raw line breaks inside quoted strings into spaces when reading, so escape them (and other control characters)
  case '\n': stream << "\\n"; break;
  case '\r': stream << "\\r"; break;
  case '\t': stream << "\\t"; break;
  default:
    if (std::iscntrl(c, std::locale::classic())) {
      // See https://yaml.org/spec/1.2.2/#escaped-characters: YAML 8-bit escapes `\xNN` contain exactly two hex digits
      stream << std::format("\\x{:02x}", static_cast<unsigned char>(c));
    }
    else {
      stream << c;
    }
  }
}

std::ostream& AppendStringLiteral(std::ostream& stream, const std::string_view str, ForceQuotes forceQuotes) {
  constexpr auto needsQuotes = [](std::string_view str) {
    // applying quotes generously even though YAML would allow more to go without quotes
    return str.empty() ||
      !std::isalpha(str.front(), std::locale::classic()) ||
      !std::ranges::all_of(str, [](char c) { return std::isalnum(c, std::locale::classic()) || c == '_' || c == ' '; });
  };
  const auto quoteOrNothing = (forceQuotes == ForceQuotes::Yes || needsQuotes(str)) ? "\"" : "";

  stream << quoteOrNothing;
  for (char c : str) {
    AppendEscaped(stream, c);
  }
  return stream << quoteOrNothing;
}

/// Recursive function to convert a JSON object to a YAML string.
/// \param column The column at which the node starts: every line of the node after the first one is prefixed with this
///        many spaces, so that it lines up with the first line
/// \note does NOT prefix the output with indentation,
///       the caller should make sure that the output stream is at the correct initial column
/// \note DOES append a newline character to the output
void SerializeJsonAsYaml(std::ostream& stream, const YamlConfig& config, nlohmann::ordered_json node, std::size_t column = {}) {
  const std::size_t spacesPerLevel = (config.indentation == WhitespaceFormat::FourSpaces) ? 4 : 2;
  const auto indent = std::string(column, ' ');

  /// does nothing on the first call and appends indentation on subsequent calls
  auto indentIfNotFirst = [first = true, &indent](std::ostream& stream) mutable {
    if (!first) { stream << indent; }
    first = false;
  };

  const auto isAtomic = [](const nlohmann::ordered_json& node) {
    return !(node.is_object() || node.is_array()) || node.empty();
  };

  if (node.is_null()) { stream << "null\n"; }
  else if (node.is_number()) { stream << node.dump() << "\n"; }
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
      SerializeJsonAsYaml(stream, config, it.value(), column + spacesPerLevel);
    }
  }
  else if (node.is_array()) {
    constexpr std::string_view marker = "- ";
    for (const auto& element : node) {
      indentIfNotFirst(stream);
      stream << marker;
      auto elementColumn = column + marker.size(); // the element continues on this line, right after the marker
      if (element.is_array() && !element.empty()) {
        if (config.includeArraySizeComments && element.size() >= config.arrayCountCommentThreshold) {
          stream << "# item count: " << element.size();
        }
        elementColumn = column + spacesPerLevel; // a nested array starts on the next line, one level deeper
        stream << "\n" << std::string(elementColumn, ' ');
      }
      SerializeJsonAsYaml(stream, config, element, elementColumn);
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
/// \details This is a small wrapper around append for convenience.
std::string to_string(const Tree& tree, const YamlConfig& config) {
  std::ostringstream stream;
  append(stream, tree, config);
  return std::move(stream).str();
}

} // namespace pep::structuredOutput::yaml
