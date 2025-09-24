#pragma once

#include <pep/accessmanager/UserMessages.hpp>

#include <pep/accessmanager/AccessManager.hpp>

namespace pep {

class AccessManager::Backend {
public:
  class Storage; // Public to allow unit testing

  struct pp_t {
    pp_t() = default;
    pp_t(PolymorphicPseudonym pp, bool isClientProvided)
      : pp(pp), isClientProvided(isClientProvided) {}
    PolymorphicPseudonym pp;
    bool isClientProvided{}; // have we seen this pp before?
  };

  Backend() = delete; // AccessManager::Backend needs a properly configured storage
  Backend(std::shared_ptr<AccessManager::Backend::Storage> storage) : mStorage(storage) {}
  Backend(const std::filesystem::path& path, std::shared_ptr<GlobalConfiguration> globalConf);

  void setAccessManager(AccessManager* accessManager) { mAccessManager = accessManager; }

  // Purely passing through to AccessManager::Backend::Storage

  void addParticipantToGroup(const LocalPseudonym& localPseudonym, const std::string& group);
  void removeParticipantFromGroup(const LocalPseudonym& localPseudonym, const std::string& group);
  void assertParticipantAccess(const std::string& userGroup, const LocalPseudonym& localPseudonym, const std::vector<std::string>& modes, Timestamp at);
  bool hasLocalPseudonym(const LocalPseudonym& localPseudonym);
  void storeLocalPseudonymAndPP(const LocalPseudonym& localPseudonym, const PolymorphicPseudonym& polymorphicPseudonym);

  /*!
  * \brief Check the basic assumptions of a ticket request, such as the existence of specified columns, no double entries, etc.
  * \param request The request to be checked.
  */
  void checkTicketRequest(const TicketRequest2& request);

  /*!
  * \brief Check whether or not the userGroup at the timestamp is granted the access modes for the participantGroups.
  * \throws Error when not all access modes are granted.
  */
  void checkParticipantGroupAccess(const std::vector<std::string>& participantGroups, const std::string& userGroup, std::vector<std::string>& modes, const Timestamp& timestamp);

  /*!
  * \brief Fill the pre_PPs vector with polymorph pseudonyms found in the participantGroup. For each participantGroup, add an entry to the participantGroupMap containing all indexes
  *        of the pps that are in the participantGroup.
  * \param pre_PPs Both an in and out parameter. Vector containing the loose requested polymorph pseudonyms. This vector is appended with the pps found in the requested participantGroups.
  */
  void fillParticipantGroupMap(const std::vector<std::string>& participantGroups, std::vector<pp_t>& pre_PPs, std::unordered_map<std::string, pep::IndexList>& participantGroupMap);

  /* !
  * \brief For each column in columns, look up the associated columngroups. Then check whether or not the userGroup has the required access to ANY of those columnGroups. If not, throw an error.
  * For each columnGroup in columnGroups, assert whether or not the userGroup has the required access modes. If so, for each columnGroup, look up all columns that are in it and add them
  *         to the columns vector. Add an entry to the columnmGroupMap containing all indexes of the columns that are in the columnGroup.
  *  \param userGroup
  *  \param columnGroups
  *  \param modes
  *  \param at timestamp of moment in time for when to assert access and columnGroup membership.
  *  \param columns Both an in and out parameter. Vector containing the loose requested columns. This vector is appended with the columns found in the requested columnGroups.
  *  \param columnGroupMap Map of IndexList (indexes of columns in columnGroup) by string (name of columnGroup).
  */
  void unfoldColumnGroupsAndAssertAccess(const std::string& userGroup, const std::vector<std::string>& columnGroups, const std::vector<std::string>& modes, Timestamp at,
                                         std::vector<std::string>& columns, std::unordered_map<std::string, pep::IndexList>& columnGroupMap);

  /*!
  * \brief Check the basic assumptions of a EncryptionKeyRequest. The accompanying ticket has to have the required access.
  * \param request The request to be checked.
  * \param ticket The ticket
  */
  void checkTicketForEncryptionKeyRequest(std::shared_ptr<EncryptionKeyRequest> request, const Ticket2& ticket);
    /*!
  * \brief Return all recorded columns, columngroups (with their columns), columngroup access rules, participantgroups, and participant access rules.
  * \param query Filtering object containing the timestamp of the query and potential filters on the entries named above.
  */
  AmaQueryResponse performAMAQuery(const AmaQuery& query, const std::string& userGroup);

  UserQueryResponse performUserQuery(const UserQuery& query, const std::string& userGroup);


  /*!
  * /brief Get all columns this userGroup has access to.
  * \param request Request stating filtering conditions.
  * \param userGroup the userGroup for which to assert access
  * \return ColumnAccess A map containing all access modes and columns by columnGroups, and a vector of columns.
  */
  ColumnAccess handleColumnAccessRequest(const ColumnAccessRequest& request, const std::string& userGroup);
   /*!
  * /brief Get all participantGroups this userGroup has access to.
  * \param request Request stating filtering conditions.
  * \param userGroup the userGroup for which to assert access
  * \return ParticipantGroupAccess A map containing all access modes by participantGroups.
  */
  ParticipantGroupAccess handleParticipantGroupAccessRequest(const ParticipantGroupAccessRequest& request, const std::string& userGroup);
  ColumnNameMappingResponse handleColumnNameMappingRequest(const ColumnNameMappingRequest& request, const std::string& userGroup);

  std::vector<StructureMetadataEntry> handleStructureMetadataRequest(
      const StructureMetadataRequest& request, const std::string& userGroup);
  void handleSetStructureMetadataRequestHead(
      const SetStructureMetadataRequest& request,
      const std::string& userGroup);
  void handleSetStructureMetadataRequestEntry(
      StructureMetadataType subjectType,
      const StructureMetadataEntry& entry,
      const std::string& userGroup);

  std::filesystem::path getStoragePath();

  std::vector<std::string> getChecksumChainNames();
  void computeChecksum(const std::string& chain, std::optional<uint64_t> maxCheckpoint, uint64_t& checksum, uint64_t& checkpoint);

  void performMutationsForRequest(const AmaMutationRequest& request, const std::string& userGroup);
  rxcpp::observable<UserMutationResponse> performUserMutationsForRequest(
    const UserMutationRequest& request, const std::string& userGroup);

  MigrateUserDbToAccessManagerResponse migrateUserDb(const std::filesystem::path& dbPath);
  void ensureNoUserData() const;
  FindUserResponse handleFindUserRequest(const FindUserRequest& request, const std::string& userGroup);

private:
  /* AMA Operations For Requests, individdual parts of performMutationsForRequest */
  void createColumnsForRequest(const AmaMutationRequest& amRequest);
  void removeColumnsForRequest(const AmaMutationRequest& amRequest);
  void createColumnGroupsForRequest(const AmaMutationRequest& amRequest);
  void removeColumnGroupsForRequest(const AmaMutationRequest& amRequest);
  void addColumnsToGroupsForRequest(const AmaMutationRequest& amRequest);
  void removeColumnsFromGroupsForRequest(const AmaMutationRequest& amRequest);
  void createParticipantGroupsForRequest(const AmaMutationRequest& amRequest);
  void removeParticipantGroupsForRequest(const AmaMutationRequest& amRequest);
  void createColumnGroupAccessRulesForRequest(const AmaMutationRequest& amRequest);
  void removeColumnGroupAccessRulesForRequest(const AmaMutationRequest& amRequest);
  void createParticipantGroupAccessRulesForRequest(const AmaMutationRequest& amRequest);
  void removeParticipantGroupAccessRulesForRequest(const AmaMutationRequest& amRequest);

  std::shared_ptr<AccessManager::Backend::Storage> mStorage;
  //We want to assign this pointer from within the constructor of access manager. shared_from_this doesn't work there, so we use a raw pointer. Should be safe, because without access manager there is also no backend.
  AccessManager* mAccessManager = nullptr;
};

}
