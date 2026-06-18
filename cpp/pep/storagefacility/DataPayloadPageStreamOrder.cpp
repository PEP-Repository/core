#include <pep/storagefacility/DataPayloadPageStreamOrder.hpp>

#include <format>
#include <stdexcept>

void pep::DataPayloadPageStreamOrder::check(const DataPayloadPage& page) {
  if (page.index_ < latestFileIndex_) {
    throw std::runtime_error(std::format(
        "Received out-of-order file: expected {}+ but got {}, page {}",
        latestFileIndex_, page.index_, page.pageNumber_));
  }
  // Next file?
  // Note: skipping (empty) files is allowed
  if (page.index_ > latestFileIndex_) {
    nextPageNumber_ = 0;
    latestFileIndex_ = page.index_;
  }

  if (nextPageNumber_ != page.pageNumber_) {
    throw std::runtime_error(std::format(
        "Received out-of-order page for file {}: expected {} but got {}",
        page.index_, nextPageNumber_, page.pageNumber_));
  }
  ++nextPageNumber_;
}
