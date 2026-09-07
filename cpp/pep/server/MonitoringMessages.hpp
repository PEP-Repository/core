#pragma once

#include <pep/auth/Signed.hpp>

namespace pep {

struct MetricsRequest {};

struct MetricsResponse {
  std::string metrics;
};

struct ChecksumChainNamesRequest {};

struct ChecksumChainNamesResponse {
  std::vector<std::string> names;
};

struct ChecksumChainRequest {
  std::string name;
  std::string checkpoint;
};

struct ChecksumChainResponse {
  std::string xorredChecksums;
  std::string checkpoint;
};

using SignedChecksumChainNamesRequest = Signed<ChecksumChainNamesRequest>;
using SignedChecksumChainRequest = Signed<ChecksumChainRequest>;
using SignedMetricsRequest = Signed<MetricsRequest>;

}
