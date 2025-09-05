#pragma once

#include <pep/crypto/Signed.hpp>

namespace pep {

class MetricsRequest {
};

class MetricsResponse {
public:
  MetricsResponse() = default;
  explicit inline MetricsResponse(const std::string& metrics) : mMetrics(metrics) { }
  std::string mMetrics;
};

class ChecksumChainNamesRequest {
};

class ChecksumChainNamesResponse {
public:
  ChecksumChainNamesResponse() = default;
  explicit inline ChecksumChainNamesResponse(const std::vector<std::string>& names) : mNames(names) { }

  std::vector<std::string> mNames;
};

class ChecksumChainRequest {
public:
  std::string mName;
  std::string mCheckpoint;
};

class ChecksumChainResponse {
public:
  ChecksumChainResponse() = default;
  inline ChecksumChainResponse(const std::string& xorredChecksums,
    const std::string& checkpoint)
    : mXorredChecksums(xorredChecksums),
    mCheckpoint(checkpoint) { }
  std::string mXorredChecksums;
  std::string mCheckpoint;
};

using SignedChecksumChainNamesRequest = Signed<ChecksumChainNamesRequest>;
using SignedChecksumChainRequest = Signed<ChecksumChainRequest>;
using SignedMetricsRequest = Signed<MetricsRequest>;

}
