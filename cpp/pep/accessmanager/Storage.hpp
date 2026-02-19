#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <set>
#include <vector>

#include <pep/accessmanager/Backend.hpp>
#include <pep/structure/ColumnName.hpp>
#include <pep/accessmanager/Records.hpp>
#include <pep/crypto/Timestamp.hpp>
#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/accessmanager/UserIdFlags.hpp>

namespace pep {

class AccessManager::Backend::Storage {
public:
  struct Implementor; // Public to allow access from checksum calculation function

private:
  std::shared_ptr<Implementor> mImplementor;
  std::shared_ptr<GlobalConfiguration> mGlobalConf;
  std::filesystem::path mStoragePath;

  std::unordered_map<LocalPseudonym, PolymorphicPseudonym> mLpToPpMap; // Use a map as checking existence and retrieval of a key takes O(1) time

  // Initialisation

  /*!
 * \brief Checks whether the database has been initialized. If the database has not been initialized, do so by hardcoded rules.
 */
  void ensureInitialized();

  /*!
 * \brief Creates/deletes automatic columns such as those for the short pseudonyms.
 * \return All columns that now exist
 */
  std::set<std::string> ensureSynced();
  void checkConfig(const std::set<std::string>& allColumns) const;
  void ensureUpToDate();
  void syncColumnGroupContents(const std::string& group, const std::set<std::string>& requiredColumns);

  /*!
  * \brief Search for any access rules and group associations pertaining to tombstoned columns, columnGroups, or participantGroups, and remove them.
  */
  void removeOrphanedRecords();

  int64_t getNextInternalUserId() const;

  /* private methods related to user groups */
  int64_t getNextUserGroupId() const;
  bool hasUserGroup(std::string_view name) const;
  bool userInGroup(int64_t internalUserId, int64_t userGroupId) const;
  std::optional<std::chrono::seconds> getMaxAuthValidity(const std::string& group, Timestamp at = TimeNow()) const;

  std::optional<int64_t> findInternalSubjectId(StructureMetadataType subjectType, std::string_view subject, Timestamp at = TimeNow()) const;
  /// Try to find the internalId for the userGroup with the given name. Throws if not found.
  int64_t getInternalSubjectId(StructureMetadataType subjectType, std::string_view subject, Timestamp at = TimeNow()) const;
  /// Find a subject for the given internalId and subjectType. If there are multiple options for a given internalId (e.g. multiple UserIds for a single user), it picks one.
  std::optional<std::string> getSubjectForInternalId(StructureMetadataType subjectType, int64_t internalId,
                                                         Timestamp at) const;

public:
  Storage(const std::filesystem::path &path, std::shared_ptr<GlobalConfiguration> globalConf);

  // Sanity checks
  std::vector<std::string> getChecksumChainNames();
  void computeChecksum(const std::string& chain, std::optional<uint64_t> maxCheckpoint,
                       uint64_t& checksum, uint64_t& checkpoint);


  std::filesystem::path getPath() {
    return mStoragePath;
  }

  /* Core operations on Participants */
  /*!
  * \brief get a vector containing all polymorphic pseudonyms currently known
  * \return The polymorphic pseudonyms
  */
  std::vector<PolymorphicPseudonym> getPPs();

  /*!
   * \brief get a vector containing all polymorphic pseudonyms belonging to the given participant groups
   * \param participantGroups Vector containing the names of participant groups. If empty, all pps of all groups are returned.
   * \return The polymorphic pseudonyms mapped to participant groups
   */
  std::unordered_map<PolymorphicPseudonym, std::unordered_set<std::string> /*participant groups*/> getPPs(const std::vector<std::string>& participantGroups);


  /*!
  * \brief Check whether a polymorphic pseudonym is stored (in memory and in storage).
  * \param localPseudonym The local pseudonym (i.e. pid\@AM) of the participant, needed to check if the pseudonym is already known,
  *                       as polymorphicPseudonym (i.e. epid = EG(r, pid, y)) will differ each time we see it
  */
  bool hasLocalPseudonym(const LocalPseudonym& localPseudonym);

  /*!
   * \brief Store a localPseudonym and a Polymorphic Pseudonym in memory and in storage.
   * \param localPseudonym The local pseudonym (i.e. pid\@AM) of the participant, needed to check if the pseudonym is already known,
   *                       as polymorphicPseudonym (i.e. epid = EG(r, pid, y)) will differ each time we see it
   * \param polymorphicPseudonym The polymorphic pseudonym to store
   */
  void storeLocalPseudonymAndPP(const LocalPseudonym& localPseudonym, const PolymorphicPseudonym& polymorphicPseudonym);


  /* Core operations on ParticipantGroups */
  bool hasParticipantGroup(const std::string& name);

  /*!
  * \brief Get all participantGroups specified in the filter, at the time of timestamp.
  * \param timestamp The moment in time which will be queried.
  * \param filter a ParticipantGroupFilter struct containing a vector of strings, indicating which participantGroups to look for.
  *               A default filter or empty filter will do no filtering and return all participantGroups known at the specified time.
  * \return A set of ParticipantGroup structs, containing the names of the found participantGroups
  */
  std::set<ParticipantGroup> getParticipantGroups(const Timestamp& timestamp, const ParticipantGroupFilter& filter = ParticipantGroupFilter()) const;
  void createParticipantGroup(const std::string& name);
  void removeParticipantGroup(const std::string& name, bool force);


  /* Core operations on ParticipantGroupParticipants */
  bool hasParticipantInGroup(const LocalPseudonym& localPseudonym, const std::string& participantGroup);
  /*!
  * \brief Get all participantGroup - Participant associations (ParticipantGroupParticipants) specified in the filter, at the time of timestamp.
  * \param timestamp The moment in time which will be queried.
  * \param filter a ParticipantGroupParticipantFilter struct containing a vector of strings for participantGroups, indicating which participantGroups to look for, and
  *               a vector of LocalPseudonyms for participants, indicating which participants to look for.
  *               A default filter or empty filter will do no filtering and return all participantGroupParticipants known at the specified time.
  * \return A set of ParticipantGroupParticipant structs, containing the name of the participantGroup and the CurvePoint of the participant.
  */
  std::set<ParticipantGroupParticipant> getParticipantGroupParticipants(const Timestamp& timestamp, const ParticipantGroupParticipantFilter& filter = ParticipantGroupParticipantFilter()) const;
  void addParticipantToGroup(const LocalPseudonym& localPseudonym, const std::string& participantGroup);
  void removeParticipantFromGroup(const LocalPseudonym& localPseudonym, const std::string& participantGroup);

  /* Core operations on ParticipantGroup Access Rules */
  bool hasParticipantGroupAccessRule(
    const std::string& group,
    const std::string& userGroup,
    const std::string& mode);
  /*!
  * \brief Get all ParticipantGroupAccessRules specified in the filter, at the time of timestamp.
  * \param timestamp The moment in time which will be queried.
  * \param filter a ParticipantGroupAccessRuleFilter
  * \return A set of ParticipantGroupAccessRules structs, containing the name of the participantGroup, name of the userGRoup, and the mode of the rule.
  */
  std::set<ParticipantGroupAccessRule> getParticipantGroupAccessRules(const Timestamp& timestamp, const ParticipantGroupAccessRuleFilter& filter = ParticipantGroupAccessRuleFilter()) const;
  void createParticipantGroupAccessRule(
    const std::string& participantGroup,
    const std::string& userGroup,
    const std::string& mode);
  void removeParticipantGroupAccessRule(
    const std::string& participantGroup,
    const std::string& userGroup,
    const std::string& mode);


  /* Core operations on Columns */
  bool hasColumn(const std::string& name);
  std::set<Column> getColumns(const Timestamp& timestamp, const ColumnFilter& filter = {}) const;
  void createColumn(const std::string& name);
  void removeColumn(const std::string& name);

  /* Core operations on ColumnGroups */
  bool hasColumnGroup(const std::string& columnGroup);
  std::set<ColumnGroup> getColumnGroups(const Timestamp& timestamp, const ColumnGroupFilter& filter = {}) const;
  void createColumnGroup(const std::string& name);
  /*!
  * \brief Attempts to put a tombstone on a ColumnGroup. When the force boolean is set True, the associated column connections (ColumnGroupColumnRecord) and access rules (ColumnGroupAccessRuleRecord)
  * are also removed. If the boolean is set False (default) and associated columns or access rules are found, a error message is shown.
  */
  void removeColumnGroup(const std::string& name, bool force);

  /* Core operations on ColumnGroupColumns */
  bool hasColumnInGroup(const std::string& column, const std::string& columnGroup);
  std::set<ColumnGroupColumn> getColumnGroupColumns(const Timestamp& timestamp, const ColumnGroupColumnFilter& filter = ColumnGroupColumnFilter()) const;
  void addColumnToGroup(const std::string& column, const std::string& columnGroup);
  void removeColumnFromGroup(const std::string& column, const std::string& columnGroup);

  /* Core operations on ColumnGroup Access Rules */
  bool hasColumnGroupAccessRule(
    const std::string& columnGroup,
    const std::string& userGroup,
    const std::string& mode);
  std::set<ColumnGroupAccessRule> getColumnGroupAccessRules(const Timestamp& timestamp, const ColumnGroupAccessRuleFilter& filter = ColumnGroupAccessRuleFilter()) const;
  void createColumnGroupAccessRule(
    const std::string& columnGroup,
    const std::string& userGroup,
    const std::string& mode);
  void removeColumnGroupAccessRule(
    const std::string& columnGroup,
    const std::string& userGroup,
    const std::string& mode);

  /* Core operations on Column Name Mappings */
  std::vector<ColumnNameMapping> getAllColumnNameMappings() const;
  std::optional<ColumnNameMapping> getColumnNameMapping(const ColumnNameSection& original) const;
  void createColumnNameMapping(const ColumnNameMapping& mapping);
  void updateColumnNameMapping(const ColumnNameMapping& mapping);
  void deleteColumnNameMapping(const ColumnNameSection& original);

  void ensureNoUserData() const;
  MigrateUserDbToAccessManagerResponse migrateUserDb(const std::filesystem::path& dbPath);

  /* Adding and removing users and user identifiers */
  int64_t createUser(std::string identifier);
  void removeUser(std::string_view uid);
  void removeUser(int64_t internalUserId);
  void addIdentifierForUser(std::string_view uid, std::string identifier, UserIdFlags flags);
  void addIdentifierForUser(int64_t internalUserId, std::string identifier, UserIdFlags flags);
  void removeIdentifierForUser(int64_t internalUserId, std::string identifier);
  void removeIdentifierForUser(std::string identifier);

  /* Finding user identifiers */
  /// Try to find the internalId for the user that has the given identifier. Returns nullopt if not found.
  std::optional<int64_t> findInternalUserId(std::string_view identifier, Timestamp at = TimeNow()) const;
  /// Try to find the internalId for the user that has the given identifier. Throws if not found.
  int64_t getInternalUserId(std::string_view identifier, Timestamp at = TimeNow()) const;
  std::optional<int64_t> findInternalUserId(const std::vector<std::string>& identifiers, Timestamp at = TimeNow()) const;
  std::unordered_set<std::string> getAllIdentifiersForUser(int64_t internalUserId, Timestamp at = TimeNow()) const;
  std::optional<std::string> getPrimaryIdentifierForUser(int64_t internalUserId, Timestamp at = TimeNow()) const;
  std::optional<std::string> getDisplayIdentifierForUser(int64_t internalUserId, Timestamp at = TimeNow()) const;

  /* Updating user identifiers */
  void setPrimaryIdentifierForUser(std::string uid);
  // Caller must make sure uid indeed belongs to internalUserId
  void setPrimaryIdentifierForUser(int64_t internalUserId, std::string uid);
  void unsetPrimaryIdentifierForUser(std::string_view uid);
  // Caller must make sure uid indeed belongs to internalUserId
  void unsetPrimaryIdentifierForUser(int64_t internalUserId, std::string_view uid);
  void setDisplayIdentifierForUser(std::string uid);
  // Caller must make sure uid indeed belongs to internalUserId
  void setDisplayIdentifierForUser(int64_t internalUserId, std::string uid);

  /* Finding userGroupIds */
  /// Try to find the internalId for the userGroup with the given name. Returns nullopt if not found.
  std::optional<int64_t> findUserGroupId(std::string_view name, Timestamp at = TimeNow()) const;
  int64_t getUserGroupId(std::string_view name, Timestamp at = TimeNow()) const;
  std::optional<std::string> getUserGroupName(int64_t userGroupId, Timestamp at) const;

  /* Check user group membership */
  std::vector<UserGroup> getUserGroupsForUser(int64_t internalUserId, Timestamp at = TimeNow()) const;
  bool userInGroup(std::string_view uid, std::string_view group) const;
  bool userInGroup(int64_t internalUserId, std::string_view group) const;
  bool userGroupIsEmpty(int64_t userGroupId) const;

  /* Manage user groups */
  int64_t createUserGroup(UserGroup userGroup);
  void modifyUserGroup(UserGroup userGroup);
  void modifyUserGroup(std::string_view name, UserGroup userGroup);
  void removeUserGroup(std::string name);
  void addUserToGroup(std::string_view uid, std::string group);
  void addUserToGroup(int64_t internalUserId, std::string group);
  void removeUserFromGroup(std::string_view uid, std::string group);
  void removeUserFromGroup(int64_t internalUserId, std::string group);

  UserQueryResponse executeUserQuery(const UserQuery& query);

  /* Core operations on Metadata */
  /// Get the metadata keys for the specified \p subject (which is of type \p subjectType)
  std::vector<StructureMetadataKey> getStructureMetadataKeys(Timestamp timestamp, StructureMetadataType subjectType, std::string_view subject) const;
  /// Get the metadata keys for the subject identified by the specified \p internalSubjectId (which is of type \p subjectType)
  std::vector<StructureMetadataKey> getStructureMetadataKeys(Timestamp timestamp, StructureMetadataType subjectType, int64_t internalSubjectId) const;
  /// Get (filtered) metadata for the specified \p subject (which is of type \p subjectType)
  std::vector<StructureMetadataEntry> getStructureMetadata(Timestamp timestamp, StructureMetadataType subjectType, const StructureMetadataFilter& filter = {}) const;
  /// Create or overwrite a metadata entry for the specified \p subject (which is of type \p subjectType)
  void setStructureMetadata(StructureMetadataType subjectType, std::string subject, StructureMetadataKey key, std::string_view value);
  /// Remove a metadata entry for the specified \p subject (which is of type \p subjectType)
  void removeStructureMetadata(StructureMetadataType subjectType, std::string subject, StructureMetadataKey key);
  /// Remove a metadata entry for the specified \p internalSubjectId (which is of type \p subjectType)
  void removeStructureMetadata(StructureMetadataType subjectType, int64_t internalSubjectId, StructureMetadataKey key);
};

}
