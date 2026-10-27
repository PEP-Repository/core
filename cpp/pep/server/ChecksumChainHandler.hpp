#pragma once

#include <pep/server/ChecksumChain.hpp>

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
  virtual const ChecksumChains& chains() const noexcept = 0;
};


class LocalChecksumChainHandler : public ChecksumChainHandler {
private:
  const ChecksumChains& chains_;

public:
  explicit LocalChecksumChainHandler(const ChecksumChains& chains, Requesters allowedRequesters = DefaultRequesters)
    : ChecksumChainHandler(allowedRequesters), chains_(chains) {
  }

  const ChecksumChains& chains() const noexcept override { return chains_; }
};


class StandaloneChecksumChainHandler : public ChecksumChainHandler {
private:
  ChecksumChains chains_;
  LocalChecksumChainHandler local_;

public:
  explicit StandaloneChecksumChainHandler(ChecksumChains chains, Requesters allowedRequesters = DefaultRequesters);

  const ChecksumChains& chains() const noexcept override { return chains_; }
};

}
