#pragma once

#include <pep/messaging/Tail.hpp>
#include <pep/server/SigningServerProxy.hpp>
#include <pep/storagefacility/DataPayloadPage.hpp>
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
  /// \brief Retrieves the paths of the current data set's pages.
  /// \return An observable emitting zero or more chunks of page paths. Every chunk is self-contained, i.e. it pushes the prefixes
  ///         that apply to its paths and pops them again, so that it can be processed (e.g. using WritePrefixedStrings) on its own.
  rxcpp::observable<PagePathResponse> requestPagePaths() const;
};

}
