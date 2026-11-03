#include <pep/async/RxRequireCount.hpp>
#include <pep/authserver/ChecksumChainForwarder.hpp>
#include <pep/server/ChecksumChain.hpp>
#include <pep/server/MonitoringMessages.hpp> // for ChecksumChainResponse

#include <rxcpp/rx-lite.hpp>

#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace pep {

namespace {

const std::unordered_map<std::string, std::string> checksumNameMappings{
    {"groups", "user-groups"}, {"user-groups-v2", "user-group-users-legacy"} };

}

std::set<std::string> ChecksumChainForwarder::chainNames() const {
  return checksumNameMappings
    | std::views::keys
    | std::ranges::to<std::set>();
}

rxcpp::observable<ChecksumChainResponse> ChecksumChainForwarder::handle(const ChecksumChainRequest& request) const {
  try {
    return am_.requestChecksumChain(
      ChecksumChainRequest{
        .name = checksumNameMappings.at(request.name),
        .checkpoint = request.checkpoint,
      }
      )
      .op(RxGetOne());
  }
  catch (const std::out_of_range&) {
    throw UnknownChecksumChainError(request.name);
  }
}

}
