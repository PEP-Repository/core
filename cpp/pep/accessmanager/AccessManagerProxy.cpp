#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/async/RxUtils.hpp>
#include <pep/messaging/ResponseToVoid.hpp>
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

rxcpp::observable<ColumnAccess> AccessManagerProxy::getAccessibleColumns(bool includeImplicitlyGranted, const std::vector<std::string>& requireModes) const {
  ColumnAccessRequest request{ includeImplicitlyGranted, requireModes };
  return this->sendRequest<ColumnAccessResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ColumnAccessResponse"));
}

rxcpp::observable<ParticipantGroupAccess> AccessManagerProxy::getAccessibleParticipantGroups(bool includeImplicitlyGranted) const {
  ParticipantGroupAccessRequest request{ includeImplicitlyGranted };
  return this->sendRequest<ParticipantGroupAccessResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ParticipantGroupAccessResponse"));
}

rxcpp::observable<ColumnNameMappingResponse> AccessManagerProxy::requestColumnNameMapping(ColumnNameMappingRequest request) const {
  return this->sendRequest<ColumnNameMappingResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ColumnNameMappingResponse"));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::migrateUserDbToAccessManager(messaging::MessageBatches fileParts) const {
  return this->sendRequest<MigrateUserDbToAccessManagerResponse>(this->sign(MigrateUserDbToAccessManagerRequest()), std::move(fileParts))
    .op(messaging::ResponseToVoid());
}

rxcpp::observable<FindUserResponse> AccessManagerProxy::requestFindUser(FindUserRequest request) const {
  return this->sendRequest<FindUserResponse>(this->sign(std::move(request)))
    .op(RxGetOne("FindUserResponse"));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::requestSetStructureMetadata(SetStructureMetadataRequest request, MessageTail<StructureMetadataEntry> entries) const {
  return this->sendRequest<SetStructureMetadataResponse>(this->sign(std::move(request)), std::move(entries))
    .op(messaging::ResponseToVoid());
}

rxcpp::observable<ColumnNameMappings> AccessManagerProxy::getColumnNameMappings() const {
  return this->requestColumnNameMapping(ColumnNameMappingRequest{})
    .map([](const ColumnNameMappingResponse& response) {
    return ColumnNameMappings(response.mappings);
      });
}

rxcpp::observable<ColumnNameMappings> AccessManagerProxy::readColumnNameMapping(const ColumnNameSection&
  original) const {
  return this
    ->requestColumnNameMapping(ColumnNameMappingRequest{ CrudAction::Read, original, std::nullopt })
    .map([](const ColumnNameMappingResponse& response) {
    return ColumnNameMappings(response.mappings);
      });
}

rxcpp::observable<ColumnNameMappings> AccessManagerProxy::createColumnNameMapping(const ColumnNameMapping&
  mapping) const {
  return this
    ->requestColumnNameMapping(ColumnNameMappingRequest{
        CrudAction::Create, mapping.original, mapping.mapped })
        .map([](const ColumnNameMappingResponse& response) {
    assert(response.mappings.size() == 1U);
    return ColumnNameMappings(response.mappings);
          });
}

rxcpp::observable<ColumnNameMappings> AccessManagerProxy::updateColumnNameMapping(const ColumnNameMapping&
  mapping) const {
  return this
    ->requestColumnNameMapping(ColumnNameMappingRequest{
        CrudAction::Update, mapping.original, mapping.mapped })
        .map([](const ColumnNameMappingResponse& response) {
    assert(response.mappings.size() == 1U);
    return ColumnNameMappings(response.mappings);
          });
}

rxcpp::observable<FakeVoid> AccessManagerProxy::deleteColumnNameMapping(const ColumnNameSection& original) const {
  return this->requestColumnNameMapping(ColumnNameMappingRequest{ CrudAction::Delete, original, std::nullopt })
    .op(messaging::ResponseToVoid<true>());
}

rxcpp::observable<StructureMetadataEntry> AccessManagerProxy::getStructureMetadata(
  StructureMetadataType subjectType,
  std::vector<std::string> subjects,
  std::vector<StructureMetadataKey> keys) const {
  StructureMetadataRequest request{ subjectType, std::move(subjects), std::move(keys) };
  return this->sendRequest<StructureMetadataEntry>(this->sign(std::move(request)));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::setStructureMetadata(StructureMetadataType subjectType, MessageTail<StructureMetadataEntry> entries) const {
  return this->requestSetStructureMetadata(SetStructureMetadataRequest{ subjectType }, std::move(entries));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::removeStructureMetadata(StructureMetadataType subjectType, std::vector<StructureMetadataSubjectKey> subjectKeys) const {
  return this->requestSetStructureMetadata(SetStructureMetadataRequest{ .subjectType = subjectType, .remove = std::move(subjectKeys) });
}

}
