#pragma once

#include <pep/server/ChecksumChain.hpp>
#include <pep/server/MonitoringMessages.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {

struct ChecksumChainResponder {
  virtual std::set<std::string> chainNames() const = 0;
  virtual rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const = 0;
};


class ChecksumChainHandler : public ChecksumChainResponder {
private:
  ChecksumChains chains_;

public:
  explicit ChecksumChainHandler(ChecksumChains chains) : chains_(std::move(chains)) {}

  std::set<std::string> chainNames() const override { return chains_.names(); }
  rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const override;
};

}
