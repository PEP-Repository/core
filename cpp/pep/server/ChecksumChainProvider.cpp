#include <pep/auth/UserGroup.hpp>
#include <pep/server/ChecksumChainProvider.hpp>

namespace pep {

const ChecksumChainProvider::Requesters ChecksumChainProvider::DefaultRequesters{ UserGroup::Watchdog };

StandaloneChecksumChainProvider::StandaloneChecksumChainProvider(ChecksumChains chains, Requesters allowedRequesters)
  : ChecksumChainProvider(chains_), // Note that we provide base class with a reference to our field...
  chains_(std::move(chains)) { // ... before initializing that field
}

}
