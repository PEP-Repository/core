#include <pep/messaging/ServerConnection.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/server/TypedClient.hpp>

namespace pep {

TypedClient::TypedClient(std::shared_ptr<messaging::ServerConnection> untyped, std::shared_ptr<const X509Identity> signingIdentity) noexcept
  : mUntyped(std::move(untyped)), mSigningIdentity(std::move(signingIdentity)) {
}

rxcpp::observable<std::string> TypedClient::requestMetrics() const {
  return this->requestSingleResponse<MetricsResponse>(this->sign(MetricsRequest()))
    .map([](MetricsResponse response) { return std::move(response).mMetrics; });
}

rxcpp::observable<std::vector<std::string>> TypedClient::requestChecksumChainNames() const {
  return this->requestSingleResponse<ChecksumChainNamesResponse>(this->sign(ChecksumChainNamesRequest()))
    .map([](ChecksumChainNamesResponse response) { return std::move(response).mNames; });
}

rxcpp::observable<ChecksumChainResponse> TypedClient::requestChecksumChain(ChecksumChainRequest request) const {
  return this->requestSingleResponse<ChecksumChainResponse>(this->sign(std::move(request)));
}

}
