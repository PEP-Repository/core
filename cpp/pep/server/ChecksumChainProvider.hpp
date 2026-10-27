#pragma once

#include <pep/server/ChecksumChain.hpp>

namespace pep {

class ChecksumChainProvider {
public:
  using Requesters = std::unordered_set<std::string>;
  static const Requesters DefaultRequesters;

private:
  Requesters allowedRequesters_;

protected:
  explicit ChecksumChainProvider(Requesters allowedRequesters = DefaultRequesters) noexcept : allowedRequesters_(std::move(allowedRequesters)) {}

public:
  virtual ~ChecksumChainProvider() noexcept = default;

  const Requesters& allowedRequesters() const noexcept { return allowedRequesters_; }
  virtual const ChecksumChains& chains() const noexcept = 0;
};


class LocalChecksumChainProvider : public ChecksumChainProvider {
private:
  const ChecksumChains& chains_;

public:
  explicit LocalChecksumChainProvider(const ChecksumChains& chains, Requesters allowedRequesters = DefaultRequesters)
    : ChecksumChainProvider(allowedRequesters), chains_(chains) {
  }

  const ChecksumChains& chains() const noexcept override { return chains_; }
};


class StandaloneChecksumChainProvider : public ChecksumChainProvider {
private:
  ChecksumChains chains_;
  LocalChecksumChainProvider local_;

public:
  explicit StandaloneChecksumChainProvider(ChecksumChains chains, Requesters allowedRequesters = DefaultRequesters);

  const ChecksumChains& chains() const noexcept override { return chains_; }
};

}
