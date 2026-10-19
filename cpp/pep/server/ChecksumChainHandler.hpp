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
protected:
  virtual const ChecksumChains& chains() const noexcept = 0;

public:
  std::set<std::string> chainNames() const override { return this->chains().names(); }
  rxcpp::observable<ChecksumChainResponse> handle(const ChecksumChainRequest& request) const override;
};


class OwningChecksumChainHandler : public LocalChecksumChainHandler {
private:
  ChecksumChains chains_;

protected:
  const ChecksumChains& chains() const noexcept override { return chains_; }

public:
  explicit OwningChecksumChainHandler(ChecksumChains chains) : chains_(std::move(chains)) {}
};


class ReferencingChecksumChainHandler : public LocalChecksumChainHandler {
private:
  const ChecksumChains& chains_;

protected:
  const ChecksumChains& chains() const noexcept override { return chains_; }

public:
  explicit ReferencingChecksumChainHandler(const ChecksumChains& chains) : chains_(chains) {}

};

}
