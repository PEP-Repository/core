#include <pep/structuredoutput/Tree.hpp>

#include <boost/property_tree/ptree.hpp>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/utils/ChronoUtil.hpp>

#include <algorithm>
#include <tuple>

namespace pep::structuredOutput {
namespace {

using nlohmann::json;
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

Tree Tree::FromUserQueryResponse(const pep::UserQueryResponse& res) {
  json root = json::object();

  // userGroups
  {
    json groups = json::array();

    for (const auto& group : res.mUserGroups) {
      json item = json::object({
        {"name", group.mName}
      });

      if (group.mMaxAuthValidity) {
        item["maxAuthValidity"] =
          pep::chrono::ToString(*group.mMaxAuthValidity);
      }

      groups.push_back(std::move(item));
    }

    root["userGroups"] = std::move(groups);
  }
  
  // users
  {
    json users = json::array();

    for (const auto& user : res.mUsers) {
      json item = json::object();

      if (user.mDisplayId) {
        item["displayId"] = *user.mDisplayId;
      }

      if (user.mPrimaryId) {
        item["primaryId"] = *user.mPrimaryId;
      }
      
      // otherIdentifiers
      {
        json ids = json::array();

        for (const auto& uid : user.mOtherUids) {
          ids.push_back(uid);
        }

        item["otherIdentifiers"] = std::move(ids);
      }

      // userGroups for users
      {
        json groups = json::array();

        for (const auto& group : user.mGroups) {
          groups.push_back(group);
        }

        item["groups"] = std::move(groups);
      }
      
      users.push_back(std::move(item));
    }
    
    root["users"] = std::move(users);
  }

  return Tree::FromJson(std::move(root));
}

} // namespace pep::structuredOutput
