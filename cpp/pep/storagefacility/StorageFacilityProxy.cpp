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
    std::optional<decltype(DataPayloadPage::mIndex)> file;
    std::optional<decltype(DataPayloadPage::mPageNumber)> page;
    XxHasher hasher = XxHasher(0);
  };
  auto ctx = std::make_shared<Context>();

  // Calculate hash of (serialized) pages as they are processed
  messaging::MessageBatches batches = pages
    .map([ctx](messaging::TailSegment<DataPayloadPage> segment) -> messaging::MessageSequence {
    return segment
      .map([ctx](DataPayloadPage page) {
      if (ctx->file.has_value() && *ctx->file > page.mIndex) { // Processing a lower file index than we did before
        throw std::runtime_error("Data payload pages out of order: can't process file " + std::to_string(page.mIndex) + " after having processed file " + std::to_string(*ctx->file));
      }
      if (!ctx->file.has_value() || page.mIndex > *ctx->file) { // First page of the next file
        ctx->file = page.mIndex;
        ctx->page.reset();
      }
      if (ctx->page.has_value() && *ctx->page + 1U != page.mPageNumber) { // Processing a page index that isn't the next one
        throw std::runtime_error("Data payload pages out of order for file " + std::to_string(*ctx->file) + ": can't process page " + std::to_string(page.mPageNumber) + " after having processed page " + std::to_string(*ctx->page));
      }
      ctx->page = page.mPageNumber;
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
