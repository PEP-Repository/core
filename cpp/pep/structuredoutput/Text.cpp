#include <pep/structuredoutput/Text.hpp>

namespace pep::structuredOutput::text {
namespace {

/// Recursive function to convert a JSON object to text format
/// @note does NOT prefix the output with indentation for the first line
/// @note DOES append a newline character to the output
void SerializeJsonAsText(std::ostream& stream, const Config& config, const nlohmann::ordered_json& node, std::size_t indentLevel = 0) {
  const std::size_t spacesPerLevel = (config.indentation == WhitespaceFormat::FourSpaces) ? 4 : 2;
  const auto indent = std::string(spacesPerLevel * indentLevel, ' ');
  
  if (node.is_null()) {
    stream << "null\n";
  }
  else if (node.is_number() || node.is_boolean()) {
    stream << node << "\n";
  }
  else if (node.is_string()) {
    stream << node.get<std::string>() << "\n";
  }
  else if (node.is_array()) {
    // Array of simple values (strings, numbers)
    bool first = true;
    for (const auto& element : node) {
      if (!first) {
        stream << indent;
      }
      first = false;
      
      if (element.is_string()) {
        stream << element.get<std::string>() << "\n";
      }
      else if (element.is_number() || element.is_boolean()) {
        stream << element << "\n";
      }
      else {
        // Nested structure
        SerializeJsonAsText(stream, config, element, indentLevel + 1);
      }
    }
  }
  else if (node.is_object()) {
    bool first = true;
    for (auto it = node.begin(); it != node.end(); ++it) {
      if (!first) {
        stream << indent;
      }
      first = false;
      
      const auto& key = it.key();
      const auto& value = it.value();
      
      // Print key
      stream << key;
      
      // Add count for arrays if configured
      if (value.is_array() && config.includeElementCounts && !value.empty()) {
        stream << " (" << value.size() << ")";
      }
      stream << ":";
      
      if (value.is_array() && value.empty()) {
        stream << " []\n";
      }
      else if (value.is_object() && value.empty()) {
        stream << " {}\n";
      }
      else if (value.is_string()) {
        stream << " " << value.get<std::string>() << "\n";
      }
      else if (value.is_number() || value.is_boolean()) {
        stream << " " << value << "\n";
      }
      else {
        stream << "\n";
        // Recursively serialize nested content with increased indentation
        stream << indent << std::string(spacesPerLevel, ' ');
        SerializeJsonAsText(stream, config, value, indentLevel + 1);
      }
    }
  }
}

} // namespace

std::ostream& append(std::ostream& stream, const Tree& tree, const Config& config) {
  SerializeJsonAsText(stream, config, tree.raw_json());
  return stream;
}

} // namespace pep::structuredOutput::text
