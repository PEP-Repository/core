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

Tree Tree::FromUserQueryResponse(const pep::UserQueryResponse& res, const UserQueryDisplayConfig& config) {
  const auto printUserGroups = HasFlags(config.flags, UserQueryDisplayConfig::Flags::PrintUserGroups);
  const auto printUsers = HasFlags(config.flags, UserQueryDisplayConfig::Flags::PrintUsers);
  const auto printUserGroupsForUsers = HasFlags(config.flags, UserQueryDisplayConfig::Flags::PrintUserGroupsForUsers);
  const auto printHeaders = HasFlags(config.flags, UserQueryDisplayConfig::Flags::PrintHeaders);
  const auto useDescriptive = config.useDescriptiveHeaders;

  json root;

  // When printHeaders is false, output bare array/value; otherwise wrap in object
  if (!printHeaders) {
    root = json::array();
  } else {
    root = json::object();
  }

  // Build userGroups array
  if (printUserGroups) {
    json groups = json::array();

    for (const auto& group : res.mUserGroups) {
      json item = json::object({
        {GetKeyName(queryKeys::name, useDescriptive), group.mName}
      });

      if (group.mMaxAuthValidity) {
        item[GetKeyName(queryKeys::maxAuthValidity, useDescriptive)] =
          pep::chrono::ToString(*group.mMaxAuthValidity);
      }

      groups.push_back(std::move(item));
    }

    if (!printHeaders) {
      root = std::move(groups);
    } else {
      root[GetKeyName(queryKeys::userGroups, useDescriptive)] = std::move(groups);
    }
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

      {
        json ids = json::array();

        for (const auto& uid : user.mOtherUids) {
          ids.push_back(uid);
        }

        item[GetKeyName(queryKeys::otherIdentifiers, useDescriptive)] = std::move(ids);
      }

      // userGroups for users
      if (printUserGroupsForUsers) {
        json groups = json::array();

        for (const auto& group : user.mGroups) {
          groups.push_back(group);
        }

        item[GetKeyName(queryKeys::groups, useDescriptive)] = std::move(groups);
      }
      
      users.push_back(std::move(item));
    }

    // add users array to root
    if (!printHeaders) {
      root = std::move(users);
    } else {
      root[GetKeyName(queryKeys::users, useDescriptive)] = std::move(users);
    }
  }

  return Tree::FromJson(std::move(root));
}

Tree Tree::FromAmaQueryResponse(const pep::AmaQueryResponse& res, const AmaQueryDisplayConfig& config) {
  const auto printColumns = HasFlags(config.flags, AmaQueryDisplayConfig::Flags::PrintColumns);
  const auto printColumnGroups = HasFlags(config.flags, AmaQueryDisplayConfig::Flags::PrintColumnGroups);
  const auto printColumnGroupAccessRules = HasFlags(config.flags, AmaQueryDisplayConfig::Flags::PrintColumnGroupAccessRules);
  const auto printParticipantGroups = HasFlags(config.flags, AmaQueryDisplayConfig::Flags::PrintParticipantGroups);
  const auto printParticipantGroupAccessRules = HasFlags(config.flags, AmaQueryDisplayConfig::Flags::PrintParticipantGroupAccessRules);
  const auto printHeaders = HasFlags(config.flags, AmaQueryDisplayConfig::Flags::PrintHeaders);
  const auto useDescriptive = config.useDescriptiveHeaders;

  // Helper to convert grouped access rules into nested JSON structure
  auto buildNestedRulesJson = [](const std::map<std::string, std::map<std::string, std::vector<std::string>>>& grouped) {
    json rulesObject = json::object();
    for (const auto& [outerKey, innerMap] : grouped) {
      json innerObject = json::object();
      for (const auto& [innerKey, modes] : innerMap) {
        json modesArray = json::array();
        for (const auto& mode : modes) {
          modesArray.push_back(mode);
        }
        innerObject[innerKey] = std::move(modesArray);
      }
      rulesObject[outerKey] = std::move(innerObject);
    }
    return rulesObject;
  };

  json root;

  // When printHeaders is false, output bare array/value, otherwise wrap in object
  if (!printHeaders) {
    root = json::array();
  } else {
    root = json::object();
  }

  // Build columns array
  if (printColumns) {
    json columnsArray = json::array();

    for (const auto& col : res.mColumns) {
      columnsArray.push_back(col.mName);
    }

    if (!printHeaders) {
      root = std::move(columnsArray);
    } else {
      root[GetKeyName(queryKeys::columns, useDescriptive)] = std::move(columnsArray);
    }
  }

  // Build column groups array
  if (printColumnGroups) {
    json columnGroupsArray = json::array();

    for (const auto& cg : res.mColumnGroups) {
      json item = json::object();
      item[GetKeyName(queryKeys::name, useDescriptive)] = cg.mName;
      
      json columnsInGroup = json::array();
      for (const auto& col : cg.mColumns) {
        columnsInGroup.push_back(col);
      }
      item[GetKeyName(queryKeys::columns, useDescriptive)] = std::move(columnsInGroup);

      columnGroupsArray.push_back(std::move(item));
    }

    if (!printHeaders) {
      root = std::move(columnGroupsArray);
    } else {
      root[GetKeyName(queryKeys::columnGroups, useDescriptive)] = std::move(columnGroupsArray);
    }
  }

  // Build column group access rules
  if (printColumnGroupAccessRules) {
    std::map<std::string, std::map<std::string, std::vector<std::string>>> grouped;
    for (const auto& rule : res.mColumnGroupAccessRules) {
      grouped[rule.mColumnGroup][rule.mAccessGroup].push_back(rule.mMode);
    }

    json rulesObject = buildNestedRulesJson(grouped);

    if (!printHeaders) {
      root = std::move(rulesObject);
    } else {
      root[GetKeyName(queryKeys::columnGroupAccessRules, useDescriptive)] = std::move(rulesObject);
    }
  }

  // Build participant groups array
  if (printParticipantGroups) {
    json groupsArray = json::array();

    for (const auto& group : res.mParticipantGroups) {
      groupsArray.push_back(group.mName);
    }

    if (!printHeaders) {
      root = std::move(groupsArray);
    } else {
      root[GetKeyName(queryKeys::participantGroups, useDescriptive)] = std::move(groupsArray);
    }
  }

  // Build participant group access rules
  if (printParticipantGroupAccessRules) {
    std::map<std::string, std::map<std::string, std::vector<std::string>>> grouped;
    for (const auto& rule : res.mParticipantGroupAccessRules) {
      grouped[rule.mParticipantGroup][rule.mUserGroup].push_back(rule.mMode);
    }

    json rulesObject = buildNestedRulesJson(grouped);

    if (!printHeaders) {
      root = std::move(rulesObject);
    } else {
      root[GetKeyName(queryKeys::participantGroupAccessRules, useDescriptive)] = std::move(rulesObject);
    }
  }

  return Tree::FromJson(std::move(root));
}



} // namespace pep::structuredOutput
