#pragma once

#include <unordered_set>
#include <optional>
#include <filesystem>

#include <pep/crypto/Timestamp.hpp>
#include <pep/authserver/AsaMessages.hpp>


namespace pep {

class AuthserverStorageBackend;

class AuthserverStorage {
private:
  std::shared_ptr<AuthserverStorageBackend> mStorage;
  std::filesystem::path mStoragePath;

  void ensureInitialized();
  void migrateUidToInternalId();

  void modifyOrCreateGroup(std::string name, const UserGroupProperties& properties, bool create);

  int64_t getNextInternalId() const;

public:
  AuthserverStorage(const std::filesystem::path& path);


  int64_t createUser(std::string identifier);
  void removeUser(std::string_view uid);
  void removeUser(int64_t internalId);
  void addIdentifierForUser(std::string_view uid, std::string identifier);
  void addIdentifierForUser(int64_t internalId, std::string identifier);
  void removeIdentifierForUser(int64_t internalId, std::string identifier);
  void removeIdentifierForUser(std::string identifier);
  std::optional<int64_t> findInternalId(std::string_view identifier, Timestamp at = Timestamp()) const;
  std::optional<int64_t> findInternalId(const std::vector<std::string>& identifiers, Timestamp at = Timestamp()) const;
  std::unordered_set<std::string> getAllIdentifiersForUser(int64_t internalId, Timestamp at = Timestamp()) const;
  std::unordered_set<std::string> getUserGroups(std::string_view uid, Timestamp at = Timestamp()) const;
  std::unordered_set<std::string> getUserGroups(int64_t internalId, Timestamp at = Timestamp()) const;
  std::optional<std::chrono::seconds> getMaxAuthValidity(const std::string& group) const;
  bool hasGroup(std::string_view name) const;
  bool userInGroup(std::string_view uid, std::string_view group) const;
  bool userInGroup(int64_t internalId, std::string_view group) const;
  void createGroup(std::string name, const UserGroupProperties& properties);
  void modifyGroup(std::string name, const UserGroupProperties& properties);
  void removeGroup(std::string name);
  void addUserToGroup(std::string_view uid, std::string group);
  void addUserToGroup(int64_t internalId, std::string group);
  void removeUserFromGroup(std::string_view uid, std::string group);
  void removeUserFromGroup(int64_t internalId, std::string group);

  AsaQueryResponse executeQuery(const AsaQuery& query);

  std::vector<std::string> getChecksumChainNames();

  void computeChecksum(const std::string& chain, std::optional<uint64_t> maxCheckpoint,
        uint64_t& checksum, uint64_t& checkpoint);

  std::filesystem::path getPath() {
    return mStoragePath;
  }
};

}
