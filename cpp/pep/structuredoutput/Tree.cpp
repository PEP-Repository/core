#include <pep/structuredoutput/Tree.hpp>

#include <boost/property_tree/ptree.hpp>
#include <pep/accessmanager/UserMessages.hpp>
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

  // Helper to get appropriate key name
  auto getKey = [useDescriptive](const queryKeys::QueryKey& key) -> std::string {
    return std::string(useDescriptive ? key.descriptive : key.simple);
  };

  json root;
  const bool outputBoth = printUserGroups && printUsers;

  // When printHeaders is false and we're outputting a single array, don't wrap in object
  if (!printHeaders && !outputBoth) {
    root = json::array();
  } else {
    root = json::object();
  }

  // Build userGroups array
  if (printUserGroups) {
    json groups = json::array();

    for (const auto& group : res.mUserGroups) {
      json item = json::object({
        {getKey(queryKeys::name), group.mName}
      });

      if (group.mMaxAuthValidity) {
        item[getKey(queryKeys::maxAuthValidity)] =
          pep::chrono::ToString(*group.mMaxAuthValidity);
      }

      groups.push_back(std::move(item));
    }

    if (!printHeaders && !outputBoth) {
      root = std::move(groups);
    } else if (printHeaders) {
      root[getKey(queryKeys::userGroups)] = std::move(groups);
    } else {
      root[std::string(queryKeys::userGroups.simple)] = std::move(groups);
    }
  }
  
  // Build users array
  if (printUsers) {
    json users = json::array();

    for (const auto& user : res.mUsers) {
      json item = json::object();

      if (user.mDisplayId) {
        item[getKey(queryKeys::displayId)] = *user.mDisplayId;
      }

      if (user.mPrimaryId) {
        item[getKey(queryKeys::primaryId)] = *user.mPrimaryId;
      }

      {
        json ids = json::array();

        for (const auto& uid : user.mOtherUids) {
          ids.push_back(uid);
        }

        item[getKey(queryKeys::otherIdentifiers)] = std::move(ids);
      }

      // userGroups for users
      if (printUserGroupsForUsers) {
        json groups = json::array();

        for (const auto& group : user.mGroups) {
          groups.push_back(group);
        }

        item[getKey(queryKeys::groups)] = std::move(groups);
      }
      
      users.push_back(std::move(item));
    }

    // add users array to root
    if (!printHeaders && !outputBoth) {
      root = std::move(users);
    } else if (printHeaders) {
      root[getKey(queryKeys::users)] = std::move(users);
    } else {
      root[std::string(queryKeys::users.simple)] = std::move(users);
    }
  }

  return Tree::FromJson(std::move(root));
}



} // namespace pep::structuredOutput
