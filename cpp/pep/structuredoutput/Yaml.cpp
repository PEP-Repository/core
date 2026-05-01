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
void SerializeJsonAsYaml(std::ostream& stream, const Config& config, nlohmann::json node, std::size_t indentLevel = {}) {
  const auto indent = std::string(2 * indentLevel, ' ');

  /// does nothing on the first call and appends indentation on subsequent calls
  auto indentIfNotFirst = [first = true, &indent](std::ostream& stream) mutable {
    if (!first) { stream << indent; }
    first = false;
  };

  const auto isAtomic = [](const nlohmann::json& node) {
    return !(node.is_object() || node.is_array()) || node.empty();
  };

  if (node.is_null()) { stream << "null\n"; }
  else if (node.is_number_integer()) { stream << std::to_string(node.get<int>()) + "\n"; }
  else if (node.is_number_float()) { stream << std::to_string(node.get<double>()) + "\n"; }
  else if (node.is_boolean()) { stream << (node.get<bool>() ? "true\n" : "false\n"); }
  else if (node.empty()) { stream << (node.is_array() ? "[]" : "{}") << "\n"; }
  else if (node.is_string()) { AppendStringLiteral(stream, node.get<std::string>(), ForceQuotes::Yes) << "\n"; }
  else if (node.is_object()) {
    for (auto it = node.begin(); it != node.end(); it++) {
      indentIfNotFirst(stream);
      AppendStringLiteral(stream, it.key(), ForceQuotes::No) << ":";

      if (isAtomic(*it)) { stream << " "; }
      else {
        if (it->is_array() && config.includeArraySizeComments) { stream << " # item count: " << it->size(); }
        stream << '\n' << indent << "  ";
      }
      SerializeJsonAsYaml(stream, config, it.value(), indentLevel + 1);
    }
  }
  else if (node.is_array()) {
    for (const auto& element : node) {
      indentIfNotFirst(stream);
      stream << "- ";

      if (element.is_array() && !element.empty()) {
        if (config.includeArraySizeComments) stream << "# item count: " << element.size();
        stream << '\n' << indent << "  ";
      }
      SerializeJsonAsYaml(stream, config, element, indentLevel + 1);
    }
  }
}

} // namespace

/// Appends a YAML representation of a tree to a stream
std::ostream& append(std::ostream& stream, const Tree& tree, const Config& config) {
  SerializeJsonAsYaml(stream, config, tree.toJson());
  return stream;
}

/// Converts a tree to string.
/// @details This is a small wrapper around append for convenience.
std::string to_string(const Tree& tree, const Config& config) {
  std::ostringstream stream;
  append(stream, tree, config);
  return std::move(stream).str();
}

/// Appends a YAML representation of a UserQuery tree to a stream, applying display config filtering
std::ostream& appendUserQuery(std::ostream& stream, const Tree& tree, const UserQueryDisplayConfig& dataFilter, const Config& formatting) {
  const auto printGroups = HasFlags(dataFilter.flags, UserQueryDisplayConfig::Flags::PrintGroups);
  const auto printUsers = HasFlags(dataFilter.flags, UserQueryDisplayConfig::Flags::PrintUsers);
  const auto printUserGroups = HasFlags(dataFilter.flags, UserQueryDisplayConfig::Flags::PrintUserGroups);
  const auto printHeaders = HasFlags(dataFilter.flags, UserQueryDisplayConfig::Flags::PrintHeaders);
  const auto useDescriptive = dataFilter.useDescriptiveHeaders;
  
  const auto& json = tree.toJson();
  
  // Create a filtered JSON based on config flags
  nlohmann::json filtered;
  
  // When printHeaders is false, output bare array/object without wrapping
  if (!printHeaders) {
    // Determine if we're outputting a single array or need an object with both
    const bool outputBoth = printGroups && printUsers;
    
    if (outputBoth) {
      filtered = nlohmann::json::object();
      if (printGroups && json.contains("userGroups")) {
        filtered["userGroups"] = json["userGroups"];
      }
      if (printUsers && json.contains("users")) {
        nlohmann::json usersData = json["users"];
        if (!printUserGroups) {
          nlohmann::json filteredUsers = nlohmann::json::array();
          for (const auto& user : usersData) {
            nlohmann::json filteredUser = user;
            filteredUser.erase("groups");
            filteredUsers.push_back(std::move(filteredUser));
          }
          usersData = std::move(filteredUsers);
        }
        filtered["users"] = std::move(usersData);
      }
    } else if (printGroups && json.contains("userGroups")) {
      filtered = json["userGroups"];
    } else if (printUsers && json.contains("users")) {
      nlohmann::json usersData = json["users"];
      if (!printUserGroups) {
        nlohmann::json filteredUsers = nlohmann::json::array();
        for (const auto& user : usersData) {
          nlohmann::json filteredUser = user;
          filteredUser.erase("groups");
          filteredUsers.push_back(std::move(filteredUser));
        }
        filtered = std::move(filteredUsers);
      } else {
        filtered = std::move(usersData);
      }
    } else {
      filtered = nlohmann::json::object();
    }
  } else {
    // When printHeaders is true, wrap with keys (descriptive or simple based on useDescriptive)
    filtered = nlohmann::json::object();
    
    if (printGroups && json.contains("userGroups")) {
      const auto key = useDescriptive 
        ? std::string(stringConstants::userGroups.descriptive)
        : "userGroups";
      filtered[key] = json["userGroups"];
    }
    
    if (printUsers && json.contains("users")) {
      nlohmann::json usersData = json["users"];
      
      if (!printUserGroups) {
        nlohmann::json filteredUsers = nlohmann::json::array();
        for (const auto& user : usersData) {
          nlohmann::json filteredUser = user;
          filteredUser.erase("groups");
          filteredUsers.push_back(std::move(filteredUser));
        }
        usersData = std::move(filteredUsers);
      }
      
      const auto key = useDescriptive
        ? std::string(stringConstants::users.descriptive)
        : "users";
      filtered[key] = std::move(usersData);
    }
  }
  
  // Apply indentation from formatting config
  const auto indent = indentations(formatting.indent);
  if (!indent.empty()) {
    stream << indent;
  }
  
  SerializeJsonAsYaml(stream, formatting, filtered);
  return stream;
}

} // namespace pep::structuredOutput::yaml
