#include <pep/auth/UserGroup.hpp>
#include <pep/server/ChecksumChainProvider.hpp>

namespace pep {

const ChecksumChainProvider::Requesters ChecksumChainProvider::DefaultRequesters{ UserGroup::Watchdog };

ChecksumChainProvider::ChecksumChainProvider(const ChecksumChains& chains, Requesters allowedRequesters) noexcept
  : chains_(chains), allowedRequesters_(std::move(allowedRequesters)) {}

StandaloneChecksumChainProvider::StandaloneChecksumChainProvider(ChecksumChains chains, Requesters allowedRequesters) noexcept
  : ChecksumChainProvider(chains_), // Note that we provide base class with a reference to our field...
  chains_(std::move(chains)) { // ... before initializing that field
}

}
