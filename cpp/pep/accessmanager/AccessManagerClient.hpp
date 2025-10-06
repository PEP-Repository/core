#pragma once

#include <pep/server/SigningServerClient.hpp>
#include <pep/accessmanager/AccessManagerMessages.hpp>
#include <pep/accessmanager/AmaMessages.hpp>
#include <pep/accessmanager/UserMessages.hpp>
#include <pep/rsk/Verifiers.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/transcryptor/KeyComponentMessages.hpp>

namespace pep {

class AccessManagerClient : public SigningServerClient {
public:
  using SigningServerClient::SigningServerClient;

  rxcpp::observable<KeyComponentResponse> requestKeyComponent(SignedKeyComponentRequest request) const;
  rxcpp::observable<SignedTicket2> requestTicket(SignedTicketRequest2 request) const;
  rxcpp::observable<IndexedTicket2> requestIndexedTicket(SignedTicketRequest2 request) const;
  rxcpp::observable<EncryptionKeyResponse> requestEncryptionKey(EncryptionKeyRequest request) const;
  rxcpp::observable<AmaMutationResponse> requestAmaMutation(AmaMutationRequest request) const;
  rxcpp::observable<AmaQueryResponse> requestAmaQuery(AmaQuery request) const;
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
  rxcpp::observable<SetStructureMetadataResponse> requestSetStructureMetadata(SetStructureMetadataRequest request, rxcpp::observable<StructureMetadataEntry> tail) const;
};

}
