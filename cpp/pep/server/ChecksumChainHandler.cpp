#pragma once

#include <pep/auth/UserGroup.hpp>
#include <pep/server/ChecksumChainHandler.hpp>
#include <pep/utils/Bitpacking.hpp>

namespace pep {

namespace {

constexpr size_t FormattedChecksumChainValueLength = 8U;

std::string ChecksumChainValueToString(uint64_t value) {
  auto result = PackUint64BE(value);
  assert(result.size() == FormattedChecksumChainValueLength);
  return result;
}

std::optional<uint64_t> ChecksumChainValueFromString(const std::string& str) {
  std::optional<uint64_t> result;
  if (!str.empty()) {
    if (str.size() != FormattedChecksumChainValueLength) {
      throw Error("checkpoint field should either be " + std::to_string(FormattedChecksumChainValueLength) + " bytes or 0");
    }
    result = UnpackUint64BE(str);
  }
  return result;
}

}

std::unordered_set<std::string> ChecksumChainHandler::allowedRequesters() const {
  return { UserGroup::Watchdog };
}

rxcpp::observable<ChecksumChainResponse> ChecksumChainProducer::handle(const ChecksumChainRequest& request) const {
  auto values = chains_.chain(request.name).get(ChecksumChainValueFromString(request.checkpoint));
  return rxcpp::observable<>::just(
    ChecksumChainResponse{
      .xorredChecksums = ChecksumChainValueToString(values.checksum),
      .checkpoint = ChecksumChainValueToString(values.checkpoint),
    }
  );
}

}
