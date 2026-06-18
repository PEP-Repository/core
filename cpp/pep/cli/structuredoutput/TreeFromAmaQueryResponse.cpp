#include <pep/cli/structuredoutput/TreeFromAmaQueryResponse.hpp>
#include <pep/structuredoutput/Common.hpp>

#include <nlohmann/json.hpp>
#include <pep/utils/ChronoUtil.hpp>
#include <pep/utils/EnumUtils.hpp>

namespace pep::structuredOutput {
namespace {

using json = nlohmann::ordered_json;
using queryKeys::GetKeyName;

}

Tree TreeFrom(const pep::AmaQueryResponse& res, const QueryDisplayConfig<AmaQueryFlags>& config) {
  const auto printColumns = HasFlags(config.flags, AmaQueryFlags::PrintColumns);
  const auto printColumnGroups = HasFlags(config.flags, AmaQueryFlags::PrintColumnGroups);
  const auto printColumnGroupAccessRules = HasFlags(config.flags, AmaQueryFlags::PrintColumnGroupAccessRules);
  const auto printParticipantGroups = HasFlags(config.flags, AmaQueryFlags::PrintParticipantGroups);
  const auto printParticipantGroupAccessRules = HasFlags(config.flags, AmaQueryFlags::PrintParticipantGroupAccessRules);
  const auto useDescriptive = config.useDescriptiveKeys;

  json root = json::object();

  // Build columns array
  if (printColumns) {
    json columnsArray = json::array();

    for (const auto& col : res.mColumns) {
      columnsArray.push_back(col.name_);
    }

    root.emplace(GetKeyName(queryKeys::columns, useDescriptive), std::move(columnsArray));
  }

  // Build column groups array
  if (printColumnGroups) {
    json columnGroupsArray = json::array();

    for (const auto& cg : res.columnGroups_) {
      json item = json::object();
      item.emplace(GetKeyName(queryKeys::name, useDescriptive), cg.name_);
      item.emplace(GetKeyName(queryKeys::columns, useDescriptive), cg.mColumns);
      columnGroupsArray.push_back(std::move(item));
    }

    root.emplace(GetKeyName(queryKeys::columnGroups, useDescriptive), std::move(columnGroupsArray));
  }

  // Build column group access rules
  if (printColumnGroupAccessRules) {
    std::map<std::tuple<std::string, std::string>, std::vector<std::string>> grouped;
    for (const auto& rule : res.mColumnGroupAccessRules) {
      grouped[{rule.mColumnGroup, rule.mAccessGroup}].push_back(rule.mMode);
    }

    json rulesArray = json::array();
    for (const auto& [key, modes] : grouped) {
      const auto& [columnGroup, accessGroup] = key;
      
      json item = json::object();
      item.emplace(GetKeyName(queryKeys::columnGroup, useDescriptive), columnGroup);
      item.emplace(GetKeyName(queryKeys::userGroup, useDescriptive), accessGroup);
      item.emplace(GetKeyName(queryKeys::mode, useDescriptive), modes);
      rulesArray.push_back(std::move(item));
    }

    root.emplace(GetKeyName(queryKeys::columnGroupAccessRules, useDescriptive), std::move(rulesArray));
  }

  // Build participant groups array
  if (printParticipantGroups) {
    json groupsArray = json::array();

    for (const auto& group : res.participantGroups_) {
      groupsArray.push_back(group.name_);
    }

    root.emplace(GetKeyName(queryKeys::participantGroups, useDescriptive), std::move(groupsArray));
  }

  // Build participant group access rules
  if (printParticipantGroupAccessRules) {
    std::map<std::tuple<std::string, std::string>, std::vector<std::string>> grouped;
    for (const auto& rule : res.mParticipantGroupAccessRules) {
      grouped[{rule.mParticipantGroup, rule.userGroup_}].push_back(rule.mMode);
    }

    json rulesArray = json::array();
    for (const auto& [key, modes] : grouped) {
      const auto& [participantGroup, userGroup] = key;
      
      json item = json::object();
      item.emplace(GetKeyName(queryKeys::participantGroup, useDescriptive), participantGroup);
      item.emplace(GetKeyName(queryKeys::userGroup, useDescriptive), userGroup);
      item.emplace(GetKeyName(queryKeys::mode, useDescriptive), modes);
      rulesArray.push_back(std::move(item));
    }

    root.emplace(GetKeyName(queryKeys::participantGroupAccessRules, useDescriptive), std::move(rulesArray));
  }

  return Tree::FromJson(std::move(root));
}

} // namespace pep::structuredOutput