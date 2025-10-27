#pragma once

#include <pep/server/SigningServerProxy.hpp>
#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/rsk/Verifiers.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/transcryptor/KeyComponentMessages.hpp>

namespace pep {

class AccessManagerProxy : public SigningServerProxy {
private:
  rxcpp::observable<FakeVoid> requestAmaMutation(AmaMutationRequest request) const;
  rxcpp::observable<FakeVoid> requestUserMutation(UserMutationRequest request) const;
  rxcpp::observable<ColumnNameMappingResponse> requestColumnNameMapping(ColumnNameMappingRequest request) const;
  rxcpp::observable<ColumnNameMapping> requestSingleColumnNameMapping(ColumnNameMappingRequest request) const;
  rxcpp::observable<FakeVoid> requestSetStructureMetadata(SetStructureMetadataRequest request, messaging::Tail<StructureMetadataEntry> entries = messaging::MakeEmptyTail<StructureMetadataEntry>()) const;

public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<KeyComponentResponse> requestKeyComponent(SignedKeyComponentRequest request) const; // Must be pre-signed because caller (who is presumably our MessageSigner) is enrolling
  rxcpp::observable<SignedTicket2> requestTicket(ClientSideTicketRequest2 request) const;
  rxcpp::observable<IndexedTicket2> requestIndexedTicket(ClientSideTicketRequest2 request) const;
  rxcpp::observable<EncryptionKeyResponse> requestEncryptionKey(EncryptionKeyRequest request) const;
  rxcpp::observable<GlobalConfiguration> requestGlobalConfiguration() const;
  rxcpp::observable<VerifiersResponse> requestVerifiers() const;

  rxcpp::observable<ColumnAccess> getAccessibleColumns(bool includeImplicitlyGranted, const std::vector<std::string>& requireModes = {}) const;
  rxcpp::observable<ParticipantGroupAccess> getAccessibleParticipantGroups(bool includeImplicitlyGranted) const;

  rxcpp::observable<FakeVoid> amaCreateColumn(std::string name) const;
  rxcpp::observable<FakeVoid> amaRemoveColumn(std::string name) const;
  rxcpp::observable<FakeVoid> amaCreateColumnGroup(std::string name) const;
  rxcpp::observable<FakeVoid> amaRemoveColumnGroup(std::string name, bool force) const;
  rxcpp::observable<FakeVoid> amaAddColumnToGroup(std::string column, std::string group) const;
  rxcpp::observable<FakeVoid> amaRemoveColumnFromGroup(std::string column, std::string group) const;
  rxcpp::observable<FakeVoid> amaCreateParticipantGroup(std::string name) const;
  rxcpp::observable<FakeVoid> amaRemoveParticipantGroup(std::string name, bool force) const;
  rxcpp::observable<FakeVoid> amaAddParticipantToGroup(std::string group, const PolymorphicPseudonym& participant) const;
  rxcpp::observable<FakeVoid> amaRemoveParticipantFromGroup(std::string group, const PolymorphicPseudonym& participant) const;
  rxcpp::observable<FakeVoid> amaCreateColumnGroupAccessRule(std::string columnGroup, std::string accessGroup, std::string mode) const;
  rxcpp::observable<FakeVoid> amaRemoveColumnGroupAccessRule(std::string columnGroup, std::string accessGroup, std::string mode) const;
  rxcpp::observable<FakeVoid> amaCreateGroupAccessRule(std::string group, std::string accessGroup, std::string mode) const;
  rxcpp::observable<FakeVoid> amaRemoveGroupAccessRule(std::string group, std::string accessGroup, std::string mode) const;
  rxcpp::observable<AmaQueryResponse> amaQuery(AmaQuery query) const;

  rxcpp::observable<FakeVoid> createUser(std::string uid) const;
  rxcpp::observable<FakeVoid> removeUser(std::string uid) const;
  rxcpp::observable<FakeVoid> addUserIdentifier(std::string existingUid, std::string newUid) const;
  rxcpp::observable<FakeVoid> removeUserIdentifier(std::string uid) const;
  rxcpp::observable<FakeVoid> createUserGroup(UserGroup userGroup) const;
  rxcpp::observable<FakeVoid> modifyUserGroup(UserGroup userGroup) const;
  rxcpp::observable<FakeVoid> removeUserGroup(std::string name) const;
  rxcpp::observable<FakeVoid> addUserToGroup(std::string uid, std::string group) const;
  rxcpp::observable<FakeVoid> removeUserFromGroup(std::string uid, std::string group, bool blockTokens) const;
  rxcpp::observable<UserQueryResponse> userQuery(UserQuery query) const;

  rxcpp::observable<ColumnNameMappings> getColumnNameMappings() const;
  rxcpp::observable<ColumnNameMappings> readColumnNameMapping(const ColumnNameSection& original) const;
  rxcpp::observable<ColumnNameMapping> createColumnNameMapping(const ColumnNameMapping& mapping) const;
  rxcpp::observable<ColumnNameMapping> updateColumnNameMapping(const ColumnNameMapping& mapping) const;
  rxcpp::observable<FakeVoid> deleteColumnNameMapping(const ColumnNameSection& original) const;

  rxcpp::observable<StructureMetadataEntry> getStructureMetadata(StructureMetadataType subjectType, std::vector<std::string> subjects, std::vector<StructureMetadataKey> keys = {}) const;
  rxcpp::observable<FakeVoid> setStructureMetadata(StructureMetadataType subjectType, messaging::Tail<StructureMetadataEntry> entries) const;
  rxcpp::observable<FakeVoid> removeStructureMetadata(StructureMetadataType subjectType, std::vector<StructureMetadataSubjectKey> subjectKeys) const;

  rxcpp::observable<FakeVoid> migrateUserDbToAccessManager(messaging::MessageBatches fileParts) const;
  rxcpp::observable<FindUserResponse> findUser(std::string primaryId, std::vector<std::string> alternativeIds) const;
};

}
