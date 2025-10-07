#include <pep/accessmanager/AccessManagerClient.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/accessmanager/AmaSerializers.hpp>
#include <pep/accessmanager/UserSerializers.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/rsk/RskSerializers.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>

namespace pep {

namespace {

bool RequestsIndexedTicket(const SignedTicketRequest2& request) { // TODO: remove
  return Serialization::FromString<TicketRequest2>(request.mData).mRequestIndexedTicket;
}

} // End anonymous namespace

rxcpp::observable<KeyComponentResponse> AccessManagerClient::requestKeyComponent(SignedKeyComponentRequest request) const {
  // TODO: consolidate duplicate code with TranscryptorClient::requestKeyComponent
  return this->sendRequest<KeyComponentResponse>(std::move(request))
    .op(RxGetOne("KeyComponentResponse"));
}

rxcpp::observable<SignedTicket2> AccessManagerClient::requestTicket(SignedTicketRequest2 request) const {
  assert(!RequestsIndexedTicket(request));
  return this->sendRequest<SignedTicket2>(std::move(request))
    .op(RxGetOne("SignedTicket2"));
}

rxcpp::observable<IndexedTicket2> AccessManagerClient::requestIndexedTicket(SignedTicketRequest2 request) const {
  assert(RequestsIndexedTicket(request));
  return this->sendRequest<IndexedTicket2>(std::move(request))
    .op(RxGetOne("IndexedTicket2"));
}

rxcpp::observable<EncryptionKeyResponse> AccessManagerClient::requestEncryptionKey(EncryptionKeyRequest request) const {
  return this->sendRequest<EncryptionKeyResponse>(this->sign(std::move(request)))
    .op(RxGetOne("EncryptionKeyResponse"));
}

rxcpp::observable<AmaMutationResponse> AccessManagerClient::requestAmaMutation(AmaMutationRequest request) const {
  return this->sendRequest<AmaMutationResponse>(this->sign(std::move(request)))
    .op(RxGetOne("AmaMutationResponse"));
}

rxcpp::observable<AmaQueryResponse> AccessManagerClient::requestAmaQuery(AmaQuery request) const {
  return this->sendRequest<AmaQueryResponse>(this->sign(std::move(request))); // TODO: recompose to a single message here
}

rxcpp::observable<UserQueryResponse> AccessManagerClient::requestUserQuery(UserQuery request) const {
  return this->sendRequest<UserQueryResponse>(this->sign(std::move(request)))
    .op(RxGetOne("UserQueryResponse"));
}

rxcpp::observable<UserMutationResponse> AccessManagerClient::requestUserMutation(UserMutationRequest request) const {
  return this->sendRequest<UserMutationResponse>(this->sign(std::move(request)))
    .op(RxGetOne("UserMutationResponse"));
}

rxcpp::observable<GlobalConfiguration> AccessManagerClient::requestGlobalConfiguration() const {
  return this->sendRequest<GlobalConfiguration>(GlobalConfigurationRequest())
    .op(RxGetOne("GlobalConfiguration"));
}

rxcpp::observable<VerifiersResponse> AccessManagerClient::requestVerifiers() const {
  return this->sendRequest<VerifiersResponse>(VerifiersRequest())
    .op(RxGetOne("VerifiersResponse"));
}

rxcpp::observable<ColumnAccessResponse> AccessManagerClient::requestColumnAccess(ColumnAccessRequest request) const {
  return this->sendRequest<ColumnAccessResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ColumnAccessResponse"));
}

rxcpp::observable<ParticipantGroupAccessResponse> AccessManagerClient::requestParticipantGroupAccess(ParticipantGroupAccessRequest request) const {
  return this->sendRequest<ParticipantGroupAccessResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ParticipantGroupAccessResponse"));
}

rxcpp::observable<ColumnNameMappingResponse> AccessManagerClient::requestColumnNameMapping(ColumnNameMappingRequest request) const {
  return this->sendRequest<ColumnNameMappingResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ColumnNameMappingResponse"));
}

rxcpp::observable<MigrateUserDbToAccessManagerResponse> AccessManagerClient::requestMigrateUserDbToAccessManager(MigrateUserDbToAccessManagerRequest request, messaging::MessageBatches parts) const {
  return this->sendRequest<MigrateUserDbToAccessManagerResponse>(this->sign(std::move(request)), std::move(parts))
    .op(RxGetOne("MigrateUserDbToAccessManagerResponse"));
}

rxcpp::observable<FindUserResponse> AccessManagerClient::requestFindUser(FindUserRequest request) const {
  return this->sendRequest<FindUserResponse>(this->sign(std::move(request)))
    .op(RxGetOne("FindUserResponse"));
}

rxcpp::observable<StructureMetadataEntry> AccessManagerClient::requestStructureMetadata(StructureMetadataRequest request) const {
  return this->sendRequest<StructureMetadataEntry>(this->sign(std::move(request)));
}

rxcpp::observable<SetStructureMetadataResponse> AccessManagerClient::requestSetStructureMetadata(SetStructureMetadataRequest request, MessageTail<StructureMetadataEntry> entries) const {
  return this->sendRequest<SetStructureMetadataResponse>(this->sign(std::move(request)), std::move(entries));
}

}
