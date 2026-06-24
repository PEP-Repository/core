#include <pep/storagefacility/DataPayloadPageStreamOrder.hpp>

#include <format>
#include <stdexcept>

void pep::DataPayloadPageStreamOrder::check(const DataPayloadPage& page) {
  if (page.index < latestFileIndex_) {
    throw std::runtime_error(std::format(
        "Received out-of-order file: expected {}+ but got {}, page {}",
        latestFileIndex_, page.index, page.pageNumber));
  }
  // Next file?
  // Note: skipping (empty) files is allowed
  if (page.index > latestFileIndex_) {
    nextPageNumber_ = 0;
    latestFileIndex_ = page.index;
  }

  if (nextPageNumber_ != page.pageNumber) {
    throw std::runtime_error(std::format(
        "Received out-of-order page for file {}: expected {} but got {}",
        page.index, nextPageNumber_, page.pageNumber));
  }
  ++nextPageNumber_;
}
