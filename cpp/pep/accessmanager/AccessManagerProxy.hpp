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
  rxcpp::observable<FakeVoid> amaRequestMutation(AmaMutationRequest request) const;

public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<KeyComponentResponse> requestKeyComponent(SignedKeyComponentRequest request) const;
  rxcpp::observable<SignedTicket2> requestTicket(SignedTicketRequest2 request) const; // TODO: don't require pre-signed
  rxcpp::observable<IndexedTicket2> requestIndexedTicket(SignedTicketRequest2 request) const; // TODO: don't require pre-signed
  rxcpp::observable<EncryptionKeyResponse> requestEncryptionKey(EncryptionKeyRequest request) const;
  rxcpp::observable<UserQueryResponse> requestUserQuery(UserQuery request) const;
  rxcpp::observable<UserMutationResponse> requestUserMutation(UserMutationRequest request) const;
  rxcpp::observable<GlobalConfiguration> requestGlobalConfiguration() const;
  rxcpp::observable<VerifiersResponse> requestVerifiers() const;
  rxcpp::observable<ColumnAccessResponse> requestColumnAccess(ColumnAccessRequest request) const;
  rxcpp::observable<ParticipantGroupAccessResponse> requestParticipantGroupAccess(ParticipantGroupAccessRequest request) const;
  rxcpp::observable<ColumnNameMappingResponse> requestColumnNameMapping(ColumnNameMappingRequest request) const;
  rxcpp::observable<MigrateUserDbToAccessManagerResponse> requestMigrateUserDbToAccessManager(MigrateUserDbToAccessManagerRequest request, messaging::MessageBatches parts) const;
  rxcpp::observable<FindUserResponse> requestFindUser(FindUserRequest request) const;
  rxcpp::observable<StructureMetadataEntry> requestStructureMetadata(StructureMetadataRequest request) const;
  rxcpp::observable<SetStructureMetadataResponse> requestSetStructureMetadata(SetStructureMetadataRequest request, MessageTail<StructureMetadataEntry> entries = MakeEmptyMessageTail<StructureMetadataEntry>()) const;

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

};

}
