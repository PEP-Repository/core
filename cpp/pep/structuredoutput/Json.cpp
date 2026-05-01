#include <pep/structuredoutput/Json.hpp>

namespace pep::structuredOutput::json {

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
  
  return stream << filtered.dump(formatting.indent);
}

} // namespace pep::structuredOutput::json
