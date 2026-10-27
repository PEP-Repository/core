#include <pep/auth/UserGroup.hpp>
#include <pep/server/ChecksumChainProvider.hpp>

namespace pep {

const ChecksumChainProvider::Requesters ChecksumChainProvider::DefaultRequesters{ UserGroup::Watchdog };

StandaloneChecksumChainProvider::StandaloneChecksumChainProvider(ChecksumChains chains, Requesters allowedRequesters)
  : ChecksumChainProvider(chains_), chains_(std::move(chains)) {
}

}
