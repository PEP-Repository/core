#pragma once

#include <pep/server/ChecksumChain.hpp>
#include <pep/server/MonitoringMessages.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {

struct ChecksumChainHandler {
  virtual ~ChecksumChainHandler() noexcept = default;

  virtual std::unordered_set<std::string> allowedRequesters() const;
  virtual std::set<std::string> chainNames() const = 0;
  virtual rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const = 0;
};


class LocalChecksumChainHandler : public ChecksumChainHandler {
private:
  const ChecksumChains& chains_;

public:
  explicit LocalChecksumChainHandler(const ChecksumChains& chains) : chains_(chains) {}

  std::set<std::string> chainNames() const override { return chains_.names(); }
  rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const override;
};

}
