#pragma once

#include <pep/storagefacility/DataPayloadPage.hpp>

namespace pep {

class DataPayloadPageStreamOrder {
  decltype(DataPayloadPage::index) latestFileIndex_ = 0;
  decltype(DataPayloadPage::pageNumber) nextPageNumber_ = 0;

public:
  DataPayloadPageStreamOrder() = default;
  DataPayloadPageStreamOrder(const DataPayloadPageStreamOrder&) = delete;
  DataPayloadPageStreamOrder& operator=(const DataPayloadPageStreamOrder&) = delete;
  DataPayloadPageStreamOrder(DataPayloadPageStreamOrder&&) = default;
  DataPayloadPageStreamOrder& operator=(DataPayloadPageStreamOrder&&) = default;

  auto latestFileIndex() const noexcept { return latestFileIndex_; }
  auto nextPageNumber() const noexcept { return nextPageNumber_; }

  /// Checks that \c DataPayloadPage::index_ is increasing compared to the previous page and
  /// that \c DataPayloadPage::pageNumber_ increments from 0 with 1 each time.
  /// \throws std::runtime_error If this is not the case
  void check(const DataPayloadPage& page);
};

}
