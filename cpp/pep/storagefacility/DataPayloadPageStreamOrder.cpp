#include <pep/storagefacility/DataPayloadPageStreamOrder.hpp>

#include <format>
#include <stdexcept>

void pep::DataPayloadPageStreamOrder::check(const DataPayloadPage& page) {
  if (page.mIndex < mLatestFileIndex) {
    throw std::runtime_error(std::format(
        "Received out-of-order file: expected {}+ but got {}, page {}",
        mLatestFileIndex, page.mIndex, page.mPageNumber));
  }
  // Next file?
  // Note: skipping (empty) files is allowed
  if (page.mIndex > mLatestFileIndex) {
    mNextPageNumber = 0;
    mLatestFileIndex = page.mIndex;
  }

  if (mNextPageNumber != page.mPageNumber) {
    throw std::runtime_error(std::format(
        "Received out-of-order page for file {}: expected {} but got {}",
        page.mIndex, mNextPageNumber, page.mPageNumber));
  }
  ++mNextPageNumber;
}
