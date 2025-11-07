#include <pep/async/RxRequireCount.hpp>
#include <pep/storagefacility/PageHash.hpp>
#include <pep/storagefacility/StorageFacilityProxy.hpp>
#include <pep/storagefacility/StorageFacilitySerializers.hpp>
#include <pep/utils/XxHasher.hpp>

namespace pep {

rxcpp::observable<DataEnumerationResponse2> StorageFacilityProxy::requestMetadataRead(MetadataReadRequest2 request) const {
  return this->sendRequest<DataEnumerationResponse2>(this->sign(std::move(request)));
}

rxcpp::observable<DataPayloadPage> StorageFacilityProxy::requestDataRead(DataReadRequest2 request) const {
  return this->sendRequest<DataPayloadPage>(this->sign(std::move(request)));
}

rxcpp::observable<DataStoreResponse2> StorageFacilityProxy::requestDataStore(DataStoreRequest2 request, messaging::Tail<DataPayloadPage> pages) const {
  struct Context {
    DataPayloadPageStreamOrder order;
    XxHasher hasher = XxHasher(0);
  };
  auto ctx = std::make_shared<Context>();

  // Calculate hash of (serialized) pages as they are processed
  messaging::MessageBatches batches = pages
    .map([ctx, numFiles = request.mEntries.size()](messaging::TailSegment<DataPayloadPage> segment) -> messaging::MessageSequence {
    return segment
      .map([ctx, numFiles](DataPayloadPage page) {

      if (page.mIndex >= numFiles) {
        throw std::runtime_error(std::format("Received out-of-bounds file index: {} >= {}",
            page.mIndex, numFiles));
      }

      ctx->order.check(page);

      auto serialized = MakeSharedCopy(Serialization::ToString(std::move(page)));
      ctx->hasher.update(PageHash(*serialized));
      return serialized;
        });
      });

  return this->sendRequest<DataStoreResponse2>(this->sign(std::move(request)), std::move(batches))
    .op(RxGetOne())
    .tap([ctx](const DataStoreResponse2& response) {
    if (response.mHash != ctx->hasher.digest()) {
      throw std::runtime_error("Returned hash from the storage facility did not match the calculated hash for the data to be stored.");
    }
      });
}

rxcpp::observable<DataDeleteResponse2> StorageFacilityProxy::requestDataDelete(DataDeleteRequest2 request) const {
  return this->sendRequest<DataDeleteResponse2>(this->sign(std::move(request)))
    .op(RxGetOne());
}

rxcpp::observable<MetadataUpdateResponse2> StorageFacilityProxy::requestMetadataStore(MetadataUpdateRequest2 request) const {
  return this->sendRequest<MetadataUpdateResponse2>(this->sign(std::move(request)))
    .op(RxGetOne());
}

rxcpp::observable<DataEnumerationResponse2> StorageFacilityProxy::requestDataEnumeration(DataEnumerationRequest2 request) const {
  return this->sendRequest<DataEnumerationResponse2>(this->sign(std::move(request)));
}

rxcpp::observable<DataHistoryResponse2> StorageFacilityProxy::requestDataHistory(DataHistoryRequest2 request) const {
  return this->sendRequest<DataHistoryResponse2>(this->sign(std::move(request)))
    .op(RxGetOne());
}

}
