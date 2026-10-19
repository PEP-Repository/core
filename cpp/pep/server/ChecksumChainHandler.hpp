#pragma once

#include <pep/server/ChecksumChain.hpp>
#include <pep/server/MonitoringMessages.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {

class ChecksumChainHandler {
public:
  using Requesters = std::unordered_set<std::string>;
  static const Requesters DefaultRequesters;

private:
  Requesters allowedRequesters_;

protected:
  explicit ChecksumChainHandler(Requesters allowedRequesters = DefaultRequesters) noexcept : allowedRequesters_(std::move(allowedRequesters)) {}

public:
  virtual ~ChecksumChainHandler() noexcept = default;

  const Requesters& allowedRequesters() const noexcept { return allowedRequesters_; }
  virtual std::set<std::string> chainNames() const = 0;
  virtual rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const = 0;
};


class LocalChecksumChainHandler : public ChecksumChainHandler {
private:
  const ChecksumChains& chains_;

public:
  explicit LocalChecksumChainHandler(const ChecksumChains& chains, Requesters allowedRequesters = DefaultRequesters)
    : ChecksumChainHandler(allowedRequesters), chains_(chains) {
  }

  std::set<std::string> chainNames() const override { return chains_.names(); }
  rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const override;
};

}
