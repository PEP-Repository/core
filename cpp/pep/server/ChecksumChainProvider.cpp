#include <pep/auth/UserGroup.hpp>
#include <pep/server/ChecksumChainProvider.hpp>
#include <pep/utils/Bitpacking.hpp>

namespace pep {

const ChecksumChainProvider::Requesters ChecksumChainProvider::DefaultRequesters{ UserGroup::Watchdog };

StandaloneChecksumChainProvider::StandaloneChecksumChainProvider(ChecksumChains chains, Requesters allowedRequesters)
  : chains_(std::move(chains)), local_(chains_) {
}

}
