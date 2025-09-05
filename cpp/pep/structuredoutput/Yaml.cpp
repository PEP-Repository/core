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
  const auto uidAndGroupOffset = indentations(withHeader ? 3 : 2);

  if (withHeader) {
    appendYamlListHeader(stream, stringConstants::users.descriptive, users.size());
  }
  for (auto& user : users) {
    stream << userOffset << "- ";
    appendYamlListHeader(stream, stringConstants::identifiersKey, user.mUids.size());
    for (auto& uid : user.mUids) {
      stream << uidAndGroupOffset << "- " << uid << "\n";
    }
    if(printUserGroups) {
      // We explicitly use 2 spaces (and not just a call to indentations with a certain indentation level),  so it aligns with "- " for the mUids header, even if we change the indentation size in indentations()
      stream << userOffset << "  ";
      appendYamlListHeader(stream, stringConstants::groupsKey, user.mGroups.size());
      for (auto& group : user.mGroups) {
        stream << uidAndGroupOffset << "- " << group << "\n";
      }
    }
    stream << "\n";
  }

  return stream;
}

} // namespace

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

} // namespace pep::structuredOutput::yaml
