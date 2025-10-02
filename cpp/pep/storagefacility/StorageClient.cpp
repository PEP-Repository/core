#include <pep/async/RxUtils.hpp>
#include <pep/storagefacility/StorageClient.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>

namespace pep {

rxcpp::observable<DataEnumerationResponse2> StorageClient::requestMetadataRead(MetadataReadRequest2 request) const {
  return this->sendRequest<DataEnumerationResponse2>(this->sign(std::move(request)));
}

rxcpp::observable<DataPayloadPage> StorageClient::requestDataRead(DataReadRequest2 request) const {
  return this->sendRequest<DataPayloadPage>(this->sign(std::move(request)));
}

rxcpp::observable<DataStoreResponse2> StorageClient::requestDataStore(DataStoreRequest2 request, MessageTail<DataPayloadPage> pages) const {
  return this->sendRequest<DataStoreResponse2>(this->sign(std::move(request)), std::move(pages))
    .op(RxGetOne("DataStoreResponse2"));
}

rxcpp::observable<DataDeleteResponse2> StorageClient::requestDataDelete(DataDeleteRequest2 request) const {
  return this->sendRequest<DataDeleteResponse2>(this->sign(std::move(request)))
    .op(RxGetOne("DataDeleteResponse2"));
}

rxcpp::observable<MetadataUpdateResponse2> StorageClient::requestMetadataStore(MetadataUpdateRequest2 request) const {
  return this->sendRequest<MetadataUpdateResponse2>(this->sign(std::move(request)))
    .op(RxGetOne("MetadataUpdateResponse2"));
}

rxcpp::observable<DataEnumerationResponse2> StorageClient::requestDataEnumeration(DataEnumerationRequest2 request) const {
  return this->sendRequest<DataEnumerationResponse2>(this->sign(std::move(request)));
}

rxcpp::observable<DataHistoryResponse2> StorageClient::requestDataHistory(DataHistoryRequest2 request) const {
  return this->sendRequest<DataHistoryResponse2>(this->sign(std::move(request)))
    .op(RxGetOne("DataHistoryResponse2"));
}

}
