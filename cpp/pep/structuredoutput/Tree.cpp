#include <pep/structuredoutput/Tree.hpp>

#include <boost/property_tree/ptree.hpp>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/utils/ChronoUtil.hpp>

#include <algorithm>
#include <tuple>

namespace pep::structuredOutput {
namespace {

using json = nlohmann::ordered_json;
using ConstRecordRef = Table::ConstRecordRef;
using OptConstRecordRef = std::optional<ConstRecordRef>;

json AsArray(ConstRecordRef record) {
  auto array = json::array();
  for (const auto& field : record) { array += field; }
  return array;
}

json ObjectFromHeaderAndRecord(ConstRecordRef header, ConstRecordRef record) {
  assert(header.size() == record.size());

  auto object = json::object();
  for (std::size_t i = 0; i < record.size(); ++i) { object.emplace(header[i], record[i]); }
  return object;
}

/// Converts the table to an array of objects (AoO)
json JsonArray(const Table& table) {
  auto array = json::array();
  for (auto record : table.records()) { array += ObjectFromHeaderAndRecord(table.header(), record); }
  return array;
}

/// Helper to get appropriate key name
std::string GetKeyName(const queryKeys::QueryKey& key, bool useDescriptive) {
  return std::string(useDescriptive ? key.descriptive : key.simple);
}

} // namespace

Tree TreeFromTable(const Table& table) {
  auto data = JsonArray(table);
  auto metadata = json::object({{"header", AsArray(table.header())}});
  return Tree::FromJson(json::object({{"metadata", std::move(metadata)}, {"data", std::move(data)}}));
}

namespace {

json PtreeToJson(const boost::property_tree::ptree& pt) {
  // Check if it's an array or object
  if (pt.empty()) {
    // if its empty its a leaf node
    return json(pt.data());
  }
  
  // if all keys are empty, it's an array
  bool isArray = true;
  for (const auto& [key, value] : pt) {
    if (!key.empty()) {
      isArray = false;
      break;
    }
  }

  if (isArray) {
    json array = json::array();
    for (const auto& [key, value] : pt) {
      array.push_back(PtreeToJson(value));
    }
    return array;
  } else {
    json object = json::object();
    for (const auto& [key, value] : pt) {
      object[key] = PtreeToJson(value);
    }
    return object;
  }
}

} // namespace

Tree Tree::FromPropertyTree(const boost::property_tree::ptree& pt) {
  return Tree::FromJson(PtreeToJson(pt));
}

Tree Tree::FromUserQueryResponse(const pep::UserQueryResponse& res, const QueryDisplayConfig<UserQueryFlags>& config) {
  const auto printUserGroups = HasFlags(config.flags, UserQueryFlags::PrintUserGroups);
  const auto printUsers = HasFlags(config.flags, UserQueryFlags::PrintUsers);
  const auto printUserGroupsForUsers = HasFlags(config.flags, UserQueryFlags::PrintUserGroupsForUsers);
  const auto useDescriptive = config.useDescriptiveKeys;

  json root = json::object();

  // Build userGroups array
  if (printUserGroups) {
    json groups = json::array();

    for (const auto& group : res.mUserGroups) {
      json item = json::object();
      item[GetKeyName(queryKeys::name, useDescriptive)] = group.mName;
      if (group.mMaxAuthValidity) {
        item[GetKeyName(queryKeys::maxAuthValidity, useDescriptive)] = pep::chrono::ToString(*group.mMaxAuthValidity);
      }
      groups.push_back(std::move(item));
    }

    root[GetKeyName(queryKeys::userGroups, useDescriptive)] = std::move(groups);
  }

  // Build users array
  if (printUsers) {
    json users = json::array();

    for (const auto& user : res.mUsers) {
      json item = json::object();

      if (user.mDisplayId) {
        item[GetKeyName(queryKeys::displayId, useDescriptive)] = *user.mDisplayId;
      }

      if (user.mPrimaryId) {
        item[GetKeyName(queryKeys::primaryId, useDescriptive)] = *user.mPrimaryId;
      }

      json otherIdsValue = json::array();
      for (const auto& uid : user.mOtherUids) {
        otherIdsValue.push_back(uid);
      }
      item[GetKeyName(queryKeys::otherIdentifiers, useDescriptive)] = otherIdsValue;

      if (printUserGroupsForUsers) {
        json userGroupsValue = json::array();
        for (const auto& group : user.mGroups) {
          userGroupsValue.push_back(group);
        }
        item[GetKeyName(queryKeys::userGroups, useDescriptive)] = userGroupsValue;
      }
      users.push_back(std::move(item));
    }

    root[GetKeyName(queryKeys::users, useDescriptive)] = std::move(users);
  }

  return Tree::FromJson(std::move(root));
}

Tree Tree::FromAmaQueryResponse(const pep::AmaQueryResponse& res, const QueryDisplayConfig<AmaQueryFlags>& config) {
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
      columnsArray.push_back(col.mName);
    }

    root[GetKeyName(queryKeys::columns, useDescriptive)] = std::move(columnsArray);
  }

  // Build column groups array
  if (printColumnGroups) {
    json columnGroupsArray = json::array();

    for (const auto& cg : res.mColumnGroups) {
      json columnsInGroup = json::array();
      for (const auto& col : cg.mColumns) {
        columnsInGroup.push_back(col);
      }
      
      json item = json::object();
      item[GetKeyName(queryKeys::name, useDescriptive)] = cg.mName;
      item[GetKeyName(queryKeys::columns, useDescriptive)] = columnsInGroup;
      columnGroupsArray.push_back(std::move(item));
    }

    root[GetKeyName(queryKeys::columnGroups, useDescriptive)] = std::move(columnGroupsArray);
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
      
      json modesArray = json::array();
      for (const auto& mode : modes) {
        modesArray.push_back(mode);
      }
      
      json item = json::object();
      item[GetKeyName(queryKeys::columnGroup, useDescriptive)] = columnGroup;
      item[GetKeyName(queryKeys::userGroup, useDescriptive)] = accessGroup;
      item[GetKeyName(queryKeys::mode, useDescriptive)] = modesArray;
      rulesArray.push_back(std::move(item));
    }

    root[GetKeyName(queryKeys::columnGroupAccessRules, useDescriptive)] = std::move(rulesArray);
  }

  // Build participant groups array
  if (printParticipantGroups) {
    json groupsArray = json::array();

    for (const auto& group : res.mParticipantGroups) {
      groupsArray.push_back(group.mName);
    }

    root[GetKeyName(queryKeys::participantGroups, useDescriptive)] = std::move(groupsArray);
  }

  // Build participant group access rules
  if (printParticipantGroupAccessRules) {
    std::map<std::tuple<std::string, std::string>, std::vector<std::string>> grouped;
    for (const auto& rule : res.mParticipantGroupAccessRules) {
      grouped[{rule.mParticipantGroup, rule.mUserGroup}].push_back(rule.mMode);
    }

    json rulesArray = json::array();
    for (const auto& [key, modes] : grouped) {
      const auto& [participantGroup, userGroup] = key;
      
      json modesArray = json::array();
      for (const auto& mode : modes) {
        modesArray.push_back(mode);
      }
      
      json item = json::object();
      item[GetKeyName(queryKeys::participantGroup, useDescriptive)] = participantGroup;
      item[GetKeyName(queryKeys::userGroup, useDescriptive)] = userGroup;
      item[GetKeyName(queryKeys::mode, useDescriptive)] = modesArray;
      rulesArray.push_back(std::move(item));
    }

    root[GetKeyName(queryKeys::participantGroupAccessRules, useDescriptive)] = std::move(rulesArray);
  }

  return Tree::FromJson(std::move(root));
}



} // namespace pep::structuredOutput
