#pragma once

#include <pep/server/ChecksumChain.hpp>

namespace pep {

class ChecksumChainProvider {
public:
  using Requesters = std::unordered_set<std::string>;
  static const Requesters DefaultRequesters;

private:
  const ChecksumChains& chains_;
  Requesters allowedRequesters_;

public:
  explicit ChecksumChainProvider(const ChecksumChains& chains, Requesters allowedRequesters = DefaultRequesters) noexcept
    : chains_(chains), allowedRequesters_(std::move(allowedRequesters)) {
  }

  virtual ~ChecksumChainProvider() noexcept = default;

  const Requesters& allowedRequesters() const noexcept { return allowedRequesters_; }
  const ChecksumChains& chains() const noexcept { return chains_; }
};


class StandaloneChecksumChainProvider : public ChecksumChainProvider {
private:
  ChecksumChains chains_;

public:
  explicit StandaloneChecksumChainProvider(ChecksumChains chains, Requesters allowedRequesters = DefaultRequesters);
};

}
