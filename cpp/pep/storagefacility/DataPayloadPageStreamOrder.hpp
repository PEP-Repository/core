#pragma once

#include <pep/storagefacility/DataPayloadPage.hpp>

namespace pep {

class DataPayloadPageStreamOrder {
  decltype(DataPayloadPage::mIndex) mLatestFileIndex = 0;
  decltype(DataPayloadPage::mPageNumber) mNextPageNumber = 0;

public:
  DataPayloadPageStreamOrder() = default;
  DataPayloadPageStreamOrder(const DataPayloadPageStreamOrder&) = delete;
  DataPayloadPageStreamOrder& operator=(const DataPayloadPageStreamOrder&) = delete;
  DataPayloadPageStreamOrder(DataPayloadPageStreamOrder&&) = default;
  DataPayloadPageStreamOrder& operator=(DataPayloadPageStreamOrder&&) = default;

  auto latestFileIndex() const noexcept { return mLatestFileIndex; }
  auto nextPageNumber() const noexcept { return mNextPageNumber; }

  /// Checks that \c DataPayloadPage::mIndex is increasing compared to the previous page and
  /// that \c DataPayloadPage::mPageNumber increments from 0 with 1 each time.
  /// \throws std::runtime_error If this is not the case
  void check(const DataPayloadPage& page);
};

}
