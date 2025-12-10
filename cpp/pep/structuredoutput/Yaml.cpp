#include <pep/structuredoutput/Yaml.hpp>

#include <pep/utils/ChronoUtil.hpp>

namespace pep::structuredOutput::yaml {
namespace {

using DisplayFlags = DisplayConfig::Flags;

std::ostream& appendYamlListHeader(std::ostream& stream, std::string_view text, std::size_t size) {
  const auto colon_unless_empty = (size != 0 ? ":" : "");
  return stream << text << colon_unless_empty << " # size=" << size << "\n";
}

std::ostream& appendYaml(std::ostream& stream,
                         const std::vector<pep::UserGroup>& group,
                         const DisplayFlags::T& flags) {
  const auto withHeader = flags & DisplayFlags::printHeaders;
  const auto groupOffset = indentations(withHeader ? 1 : 0);

  if (withHeader) {
    stream << "- ";
    appendYamlListHeader(stream, stringConstants::userGroups.descriptive, group.size());
  }
  for (auto& g : group) {
    const auto& gMaxAuth = g.mMaxAuthValidity;

    stream << groupOffset << "- " << g.mName;
    if (gMaxAuth) {
      stream << ": {" << stringConstants::maxAuthValidityKey << ": " << pep::chrono::ToString(*gMaxAuth) << "}";
    }
    stream << "\n";
  }

  return stream;
}

std::ostream& appendYaml(std::ostream& stream,
                         const std::vector<pep::QRUser>& users,
                         const DisplayFlags::T& flags) {
  const auto printUserGroups = flags & DisplayConfig::Flags::printUserGroups;
  const auto withHeader = flags & DisplayFlags::printHeaders;
  const auto userOffset = indentations(withHeader ? 1 : 0);
  const auto userInnerOffset = userOffset + "  "; //We don't use indentations(), but hardcode the extra indent to two spaces, so it matches the "- " of the first line of the user output.
  const auto uidAndGroupOffset = indentations(withHeader ? 3 : 2);

  if (withHeader) {
    appendYamlListHeader(stream, stringConstants::users.descriptive, users.size());
  }
  for (auto& user : users) {
    stream << userOffset << "- ";
    bool hasWritten = false;
    if (user.mDisplayId) {
      hasWritten = true;
      stream << stringConstants::displayIdKey << ": " << *user.mDisplayId << "\n";
    }
    if (user.mPrimaryId) {
      if (hasWritten)
        stream << userInnerOffset;
      hasWritten = true;
      stream << stringConstants::primaryIdKey << ": " << *user.mPrimaryId << "\n";
    }
    if (hasWritten)
      stream << userInnerOffset;
    appendYamlListHeader(stream, stringConstants::otherIdentifiersKey, user.mOtherUids.size());
    for (auto& uid : user.mOtherUids) {
      stream << uidAndGroupOffset << "- " << uid << "\n";
    }
    if(printUserGroups) {
      stream << userInnerOffset;
      appendYamlListHeader(stream, stringConstants::groupsKey, user.mGroups.size());
      for (auto& group : user.mGroups) {
        stream << uidAndGroupOffset << "- " << group << "\n";
      }
    }
    stream << "\n";
  }

  return stream;
}

std::ostream& AppendStringLiteral(std::ostream& stream, const std::string_view str) {
  constexpr auto isSpecial = [](char c) {
    constexpr auto specialChars = std::string_view{"\\\""};
    return specialChars.find(c) == specialChars.size();
  };
  constexpr auto escapeChar = '\\';

  stream << '"';
  for (char c : str) {
    if (isSpecial(c)) { stream << escapeChar; }
    stream << c;
  }
  return stream << '"';
}

/// Recursive function to convert a JSON object to a YAML string.
/// @note does NOT prefix the output with indentation,
///       the caller should make sure that the output stream is at the correct initial indentation level
/// @note DOES append a newline character to the output
void SerializeJsonAsYaml(std::ostream& stream, nlohmann::json node, std::size_t indentLevel = {}) {
  const auto indent = std::string(2 * indentLevel, ' ');
  const auto isAtomic = [](const nlohmann::json& node) {
    return !(node.is_object() || node.is_array()) || node.empty();
  };
  auto indentIfNotFirst = [first = true, &indent](std::ostream& stream) mutable {
    if (!first) { stream << indent; }
    first = false;
  };

  if (node.is_object()) {
    if (node.empty()) { stream << "{}\n"; }
    else for (auto it = node.begin(); it != node.end(); ++it) {
      indentIfNotFirst(stream);
      stream << it.key() << ":";

      if (isAtomic(*it)) { stream << " "; }
      else {
        if (it->is_array()) { stream << " # size = " << it->size(); }
        stream << '\n' << indent << "  ";
      }
      SerializeJsonAsYaml(stream, it.value(), indentLevel + 1);
    }
  }
  else if (node.is_array()) {
    if (node.empty()) { stream << "[]\n"; }
    else for (const auto& element : node) {
      indentIfNotFirst(stream);
      stream << "- ";

      if (element.is_array() && !element.empty()) {
        stream << "# size = " << element.size();
        stream << '\n' << indent << "  ";
      }
      SerializeJsonAsYaml(stream, element, indentLevel + 1);
    }
  }
  else if (node.is_string()) {
    AppendStringLiteral(stream, node.get<std::string>());
    stream << "\n";
  }
  else if (node.is_number_integer()) { stream << std::to_string(node.get<int>()) + "\n"; }
  else if (node.is_number_float()) { stream << std::to_string(node.get<double>()) + "\n"; }
  else if (node.is_boolean()) { stream << (node.get<bool>() ? "true\n" : "false\n"); }
  else if (node.is_null()) { stream << "null\n"; }
}

} // namespace

/// Appends a YAML representation of a tree to a stream
std::ostream& append(std::ostream& stream, const Tree& tree) {
  SerializeJsonAsYaml(stream, tree.toJson());
  return stream;
}

std::ostream& append(std::ostream& stream, const pep::UserQueryResponse& res, DisplayConfig config) {
  const auto printGroups = config.flags & DisplayConfig::Flags::printGroups;
  const auto printUsers = config.flags & DisplayConfig::Flags::printUsers;

  if (printGroups) {
    appendYaml(stream, res.mUserGroups, config.flags);
  }
  if (printGroups && printUsers) {
    stream << "\n"; // empty line between data groups
  }
  if (printUsers) {
    appendYaml(stream, res.mUsers, config.flags);
  }

  return stream;
}
/// Converts a tree to string.
/// @details This is a small wrapper around append for convenience.
std::string to_string(const Tree& tree) {
  std::ostringstream stream;
  append(stream, tree);
  std::cout << stream.str() << std::endl;
  return std::move(stream).str();
}

} // namespace pep::structuredOutput::yaml
