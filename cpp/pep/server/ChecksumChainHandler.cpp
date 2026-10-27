#include <pep/auth/UserGroup.hpp>
#include <pep/server/ChecksumChainHandler.hpp>
#include <pep/utils/Bitpacking.hpp>

namespace pep {

const ChecksumChainHandler::Requesters ChecksumChainHandler::DefaultRequesters{ UserGroup::Watchdog };

StandaloneChecksumChainHandler::StandaloneChecksumChainHandler(ChecksumChains chains, Requesters allowedRequesters)
  : chains_(std::move(chains)), local_(chains_) {
}

}
