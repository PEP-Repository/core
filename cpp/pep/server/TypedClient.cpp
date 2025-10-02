#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/server/TypedClient.hpp>

namespace pep {

TypedClient::TypedClient(std::shared_ptr<messaging::ServerConnection> untyped, std::shared_ptr<const X509Identity> signingIdentity) noexcept
  : mUntyped(std::move(untyped)), mSigningIdentity(std::move(signingIdentity)) {
  assert(mUntyped != nullptr);
  assert(mSigningIdentity != nullptr);
}

rxcpp::observable<ConnectionStatus> TypedClient::connectionStatus() const {
  return mUntyped->connectionStatus();
}

rxcpp::observable<VersionResponse> TypedClient::requestVersion() const {
  return this->requestSingleResponse<VersionResponse>(VersionRequest());
}

rxcpp::observable<MetricsResponse> TypedClient::requestMetrics() const {
  return this->requestSingleResponse<MetricsResponse>(this->sign(MetricsRequest()));
}

rxcpp::observable<ChecksumChainNamesResponse> TypedClient::requestChecksumChainNames() const {
  return this->requestSingleResponse<ChecksumChainNamesResponse>(this->sign(ChecksumChainNamesRequest()));
}

rxcpp::observable<ChecksumChainResponse> TypedClient::requestChecksumChain(ChecksumChainRequest request) const {
  return this->requestSingleResponse<ChecksumChainResponse>(this->sign(std::move(request)));
}

}
