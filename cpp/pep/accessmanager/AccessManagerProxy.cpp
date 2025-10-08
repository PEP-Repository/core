#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/rsk/RskSerializers.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>
#include <pep/transcryptor/KeyComponentSerializers.hpp>

namespace pep {

namespace {

[[maybe_unused]] bool RequestsIndexedTicket(const SignedTicketRequest2& request) { // TODO: remove
  return Serialization::FromString<TicketRequest2>(request.mData).mRequestIndexedTicket;
}

} // End anonymous namespace

rxcpp::observable<KeyComponentResponse> AccessManagerProxy::requestKeyComponent(SignedKeyComponentRequest request) const {
  // TODO: consolidate duplicate code with TranscryptorProxy::requestKeyComponent
  return this->sendRequest<KeyComponentResponse>(std::move(request))
    .op(RxGetOne("KeyComponentResponse"));
}

rxcpp::observable<SignedTicket2> AccessManagerProxy::requestTicket(SignedTicketRequest2 request) const {
  assert(!RequestsIndexedTicket(request));
  return this->sendRequest<SignedTicket2>(std::move(request))
    .op(RxGetOne("SignedTicket2"));
}

rxcpp::observable<IndexedTicket2> AccessManagerProxy::requestIndexedTicket(SignedTicketRequest2 request) const {
  assert(RequestsIndexedTicket(request));
  return this->sendRequest<IndexedTicket2>(std::move(request))
    .op(RxGetOne("IndexedTicket2"));
}

rxcpp::observable<EncryptionKeyResponse> AccessManagerProxy::requestEncryptionKey(EncryptionKeyRequest request) const {
  return this->sendRequest<EncryptionKeyResponse>(this->sign(std::move(request)))
    .op(RxGetOne("EncryptionKeyResponse"));
}

rxcpp::observable<GlobalConfiguration> AccessManagerProxy::requestGlobalConfiguration() const {
  return this->sendRequest<GlobalConfiguration>(GlobalConfigurationRequest())
    .op(RxGetOne("GlobalConfiguration"));
}

rxcpp::observable<VerifiersResponse> AccessManagerProxy::requestVerifiers() const {
  return this->sendRequest<VerifiersResponse>(VerifiersRequest())
    .op(RxGetOne("VerifiersResponse"));
}

rxcpp::observable<ColumnAccessResponse> AccessManagerProxy::requestColumnAccess(ColumnAccessRequest request) const {
  return this->sendRequest<ColumnAccessResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ColumnAccessResponse"));
}

rxcpp::observable<ParticipantGroupAccessResponse> AccessManagerProxy::requestParticipantGroupAccess(ParticipantGroupAccessRequest request) const {
  return this->sendRequest<ParticipantGroupAccessResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ParticipantGroupAccessResponse"));
}

rxcpp::observable<ColumnNameMappingResponse> AccessManagerProxy::requestColumnNameMapping(ColumnNameMappingRequest request) const {
  return this->sendRequest<ColumnNameMappingResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ColumnNameMappingResponse"));
}

rxcpp::observable<MigrateUserDbToAccessManagerResponse> AccessManagerProxy::requestMigrateUserDbToAccessManager(MigrateUserDbToAccessManagerRequest request, messaging::MessageBatches parts) const {
  return this->sendRequest<MigrateUserDbToAccessManagerResponse>(this->sign(std::move(request)), std::move(parts))
    .op(RxGetOne("MigrateUserDbToAccessManagerResponse"));
}

rxcpp::observable<FindUserResponse> AccessManagerProxy::requestFindUser(FindUserRequest request) const {
  return this->sendRequest<FindUserResponse>(this->sign(std::move(request)))
    .op(RxGetOne("FindUserResponse"));
}

rxcpp::observable<StructureMetadataEntry> AccessManagerProxy::requestStructureMetadata(StructureMetadataRequest request) const {
  return this->sendRequest<StructureMetadataEntry>(this->sign(std::move(request)));
}

rxcpp::observable<SetStructureMetadataResponse> AccessManagerProxy::requestSetStructureMetadata(SetStructureMetadataRequest request, MessageTail<StructureMetadataEntry> entries) const {
  return this->sendRequest<SetStructureMetadataResponse>(this->sign(std::move(request)), std::move(entries));
}

}
