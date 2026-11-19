#pragma once

#include <pep/server/SigningServerProxy.hpp>
#include <pep/storagefacility/DataPayloadPage.hpp>
#include <pep/storagefacility/RedactionMessages.hpp>
#include <pep/storagefacility/StorageFacilityMessages.hpp>

namespace pep {

class StorageFacilityProxy : public SigningServerProxy {
public:
  using SigningServerProxy::SigningServerProxy;

  rxcpp::observable<DataEnumerationResponse2> requestMetadataRead(MetadataReadRequest2 request) const;
  rxcpp::observable<DataPayloadPage> requestDataRead(DataReadRequest2 request) const;
  rxcpp::observable<DataStoreResponse2> requestDataStore(DataStoreRequest2 request, messaging::Tail<DataPayloadPage> pages) const;
  rxcpp::observable<DataDeleteResponse2> requestDataDelete(DataDeleteRequest2 request) const;
  rxcpp::observable<MetadataUpdateResponse2> requestMetadataStore(MetadataUpdateRequest2 request) const;
  rxcpp::observable<DataEnumerationResponse2> requestDataEnumeration(DataEnumerationRequest2 request) const;
  rxcpp::observable<DataHistoryResponse2> requestDataHistory(DataHistoryRequest2 request) const;
  rxcpp::observable<DataSizeResponse> requestDataSize(DataSizeRequest request) const;
  rxcpp::observable<PagePathResponse> requestPagePaths() const;

  rxcpp::observable<RedactionCycle> listRedactionCycles() const;
  rxcpp::observable<RedactionCycle> announceRedactionCycle(RedactionCycle::Definition definition) const;
  rxcpp::observable<RedactionCycle> cancelRedactionCycle(RedactionCycle::Id id) const;
  rxcpp::observable<RedactionCycle> confirmRedactionCycle(RedactionCycle::Id id) const;
};

}
