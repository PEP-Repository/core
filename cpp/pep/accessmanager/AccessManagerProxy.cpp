#include <pep/accessmanager/AccessManagerProxy.hpp>
#include <pep/accessmanager/AccessManagerSerializers.hpp>
#include <pep/messaging/ResponseToVoid.hpp>
#include <pep/rsk/RskSerializers.hpp>
#include <pep/structure/StructureSerializers.hpp>
#include <pep/ticketing/TicketingSerializers.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>

namespace pep {

rxcpp::observable<SignedTicket2> AccessManagerProxy::requestTicket(ClientSideTicketRequest2 request) const {
  TicketRequest2 sendable{
    std::move(request),
    false // mRequestIndexedTicket
  };
  return this->sendRequest<SignedTicket2>(this->sign(std::move(sendable)))
    .op(RxGetOne());
}

rxcpp::observable<IndexedTicket2> AccessManagerProxy::requestIndexedTicket(ClientSideTicketRequest2 request) const {
  TicketRequest2 sendable{
    std::move(request),
    true // mRequestIndexedTicket
  };
  return this->sendRequest<IndexedTicket2>(this->sign(std::move(sendable)))
    .op(RxGetOne());
}

rxcpp::observable<EncryptionKeyResponse> AccessManagerProxy::requestEncryptionKey(EncryptionKeyRequest request) const {
  return this->sendRequest<EncryptionKeyResponse>(this->sign(std::move(request)))
    .op(RxGetOne());
}

rxcpp::observable<GlobalConfiguration> AccessManagerProxy::requestGlobalConfiguration() const {
  return this->sendRequest<GlobalConfiguration>(GlobalConfigurationRequest())
    .op(RxGetOne());
}

rxcpp::observable<VerifiersResponse> AccessManagerProxy::requestVerifiers() const {
  return this->sendRequest<VerifiersResponse>(VerifiersRequest())
    .op(RxGetOne());
}

rxcpp::observable<ColumnAccess> AccessManagerProxy::getAccessibleColumns(bool includeImplicitlyGranted, const std::vector<std::string>& requireModes) const {
  ColumnAccessRequest request{ includeImplicitlyGranted, requireModes };
  return this->sendRequest<ColumnAccessResponse>(this->sign(std::move(request)))
    .op(RxGetOne());
}

rxcpp::observable<ParticipantGroupAccess> AccessManagerProxy::getAccessibleParticipantGroups(bool includeImplicitlyGranted) const {
  ParticipantGroupAccessRequest request{ includeImplicitlyGranted };
  return this->sendRequest<ParticipantGroupAccessResponse>(this->sign(std::move(request))) //NOLINT(performance-move-const-arg) may change
    .op(RxGetOne());
}

rxcpp::observable<ColumnNameMappingResponse> AccessManagerProxy::requestColumnNameMapping(ColumnNameMappingRequest request) const {
  return this->sendRequest<ColumnNameMappingResponse>(this->sign(std::move(request)))
    .op(RxGetOne());
}

rxcpp::observable<ColumnNameMapping> AccessManagerProxy::requestSingleColumnNameMapping(ColumnNameMappingRequest request) const {
  return this->requestColumnNameMapping(std::move(request))
    .map([](ColumnNameMappingResponse response) {
    if (response.mappings.size() != 1U) {
      throw std::runtime_error("Expected exactly 1 column name mapping but received " + std::to_string(response.mappings.size()));
    }
    return std::move(response.mappings.front());
      });
}

rxcpp::observable<FakeVoid> AccessManagerProxy::migrateUserDbToAccessManager(messaging::MessageBatches fileParts) const {
  return this->sendRequest<MigrateUserDbToAccessManagerResponse>(this->sign(MigrateUserDbToAccessManagerRequest()), std::move(fileParts))
    .op(messaging::ResponseToVoid());
}

rxcpp::observable<FindUserResponse> AccessManagerProxy::findUser(std::string primaryId, std::vector<std::string> alternativeIds) const {
  FindUserRequest request(std::move(primaryId), std::move(alternativeIds));
  return this->sendRequest<FindUserResponse>(this->sign(std::move(request)))
    .op(RxGetOne());
}

rxcpp::observable<FakeVoid> AccessManagerProxy::requestSetStructureMetadata(SetStructureMetadataRequest request, messaging::Tail<StructureMetadataEntry> entries) const {
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

rxcpp::observable<ColumnNameMapping> AccessManagerProxy::createColumnNameMapping(const ColumnNameMapping&
  mapping) const {
  return this
    ->requestSingleColumnNameMapping(ColumnNameMappingRequest{
        CrudAction::Create, mapping.original, mapping.mapped });
}

rxcpp::observable<ColumnNameMapping> AccessManagerProxy::updateColumnNameMapping(const ColumnNameMapping&
  mapping) const {
  return this
    ->requestSingleColumnNameMapping(ColumnNameMappingRequest{
        CrudAction::Update, mapping.original, mapping.mapped });
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

rxcpp::observable<FakeVoid> AccessManagerProxy::setStructureMetadata(StructureMetadataType subjectType, messaging::Tail<StructureMetadataEntry> entries) const {
  return this->requestSetStructureMetadata(SetStructureMetadataRequest{ subjectType }, std::move(entries));
}

rxcpp::observable<FakeVoid> AccessManagerProxy::removeStructureMetadata(StructureMetadataType subjectType, std::vector<StructureMetadataSubjectKey> subjectKeys) const {
  return this->requestSetStructureMetadata(SetStructureMetadataRequest{ .subjectType = subjectType, .remove = std::move(subjectKeys) });
}

}
