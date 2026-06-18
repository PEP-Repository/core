#pragma once

#include <pep/auth/Signed.hpp>

namespace pep {

class MetricsRequest {
};

class MetricsResponse {
public:
  MetricsResponse() = default;
  explicit inline MetricsResponse(const std::string& metrics) : metrics_(metrics) { }
  std::string metrics_;
};

class ChecksumChainNamesRequest {
};

class ChecksumChainNamesResponse {
public:
  ChecksumChainNamesResponse() = default;
  explicit inline ChecksumChainNamesResponse(const std::vector<std::string>& names) : names_(names) { }

  std::vector<std::string> names_;
};

class ChecksumChainRequest {
public:
  std::string name_;
  std::string checkpoint_;
};

class ChecksumChainResponse {
public:
  ChecksumChainResponse() = default;
  inline ChecksumChainResponse(const std::string& xorredChecksums,
    const std::string& checkpoint)
    : xorredChecksums_(xorredChecksums),
    checkpoint_(checkpoint) { }
  std::string xorredChecksums_;
  std::string checkpoint_;
};

using SignedChecksumChainNamesRequest = Signed<ChecksumChainNamesRequest>;
using SignedChecksumChainRequest = Signed<ChecksumChainRequest>;
using SignedMetricsRequest = Signed<MetricsRequest>;

}
