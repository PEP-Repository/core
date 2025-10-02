#include <pep/storagefacility/StorageClient.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>

namespace pep {

rxcpp::observable<DataEnumerationResponse2> StorageClient::requestMetadataRead(MetadataReadRequest2 request) const {
  return this->requestResponseSequence<DataEnumerationResponse2>(this->sign(std::move(request)));
}

rxcpp::observable<DataPayloadPage> StorageClient::requestDataRead(DataReadRequest2 request) const {
  return this->requestResponseSequence<DataPayloadPage>(this->sign(std::move(request)));
}

rxcpp::observable<DataStoreResponse2> StorageClient::requestDataStore(DataStoreRequest2 request, MessageTail<DataPayloadPage> pages) const {
  return this->requestSingleResponse<DataStoreResponse2>(this->sign(std::move(request)), std::move(pages));
}

rxcpp::observable<DataDeleteResponse2> StorageClient::requestDataDelete(DataDeleteRequest2 request) const {
  return this->requestSingleResponse<DataDeleteResponse2>(this->sign(std::move(request)));
}

rxcpp::observable<MetadataUpdateResponse2> StorageClient::requestMetadataStore(MetadataUpdateRequest2 request) const {
  return this->requestSingleResponse<MetadataUpdateResponse2>(this->sign(std::move(request)));
}

rxcpp::observable<DataEnumerationResponse2> StorageClient::requestDataEnumeration(DataEnumerationRequest2 request) const {
  return this->requestResponseSequence<DataEnumerationResponse2>(this->sign(std::move(request)));
}

rxcpp::observable<DataHistoryResponse2> StorageClient::requestDataHistory(DataHistoryRequest2 request) const {
  return this->requestSingleResponse<DataHistoryResponse2>(this->sign(std::move(request)));
}

}
