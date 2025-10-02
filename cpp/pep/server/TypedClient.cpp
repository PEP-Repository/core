#include <pep/async/RxUtils.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/server/TypedClient.hpp>

namespace pep {

TypedClient::TypedClient(std::shared_ptr<messaging::ServerConnection> untyped, const MessageSigner& messageSigner) noexcept
  : mUntyped(std::move(untyped)), mMessageSigner(messageSigner) {
  assert(mUntyped != nullptr);
}

rxcpp::observable<ConnectionStatus> TypedClient::connectionStatus() const {
  return mUntyped->connectionStatus();
}

rxcpp::observable<FakeVoid> TypedClient::shutdown() {
  return mUntyped->shutdown();
}

rxcpp::observable<VersionResponse> TypedClient::requestVersion() const {
  return this->sendRequest<VersionResponse>(VersionRequest())
    .op(RxGetOne("VersionResponse"));
}

rxcpp::observable<MetricsResponse> TypedClient::requestMetrics() const {
  return this->sendRequest<MetricsResponse>(this->sign(MetricsRequest()))
    .op(RxGetOne("MetricsResponse"));
}

rxcpp::observable<ChecksumChainNamesResponse> TypedClient::requestChecksumChainNames() const {
  return this->sendRequest<ChecksumChainNamesResponse>(this->sign(ChecksumChainNamesRequest()))
    .op(RxGetOne("ChecksumChainNamesResponse"));
}

rxcpp::observable<ChecksumChainResponse> TypedClient::requestChecksumChain(ChecksumChainRequest request) const {
  return this->sendRequest<ChecksumChainResponse>(this->sign(std::move(request)))
    .op(RxGetOne("ChecksumChainResponse"));
}

}
