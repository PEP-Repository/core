#pragma once

#include <pep/server/ChecksumChain.hpp>
#include <pep/server/MonitoringMessages.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {

struct ChecksumChainResponder {
  virtual std::set<std::string> chainNames() const = 0;
  virtual rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const = 0;
};

}
