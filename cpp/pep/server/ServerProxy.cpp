#include <pep/async/RxRequireCount.hpp>
#include <pep/messaging/MessagingSerializers.hpp>
#include <pep/server/MonitoringSerializers.hpp>
#include <pep/server/ServerProxy.hpp>

namespace pep {

namespace {

[[noreturn]] void ThrowInvalidResponse(const std::string& error, const std::type_info& requestInfo, const std::type_info& responseInfo, const std::string& epilogue = std::string()) {
  throw std::runtime_error(error
    + " in response to request " + boost::core::demangle(requestInfo.name())
    + ": expected " + boost::core::demangle(responseInfo.name())
    + epilogue);
}

}

void ServerProxy::ValidateResponse(MessageMagic magic, const std::string& response, const std::type_info& responseInfo, const std::type_info& requestInfo) {
  Error::ThrowIfDeserializable(response);
  if (response.size() < sizeof(MessageMagic)) {
    ThrowInvalidResponse("Unexpected short message", requestInfo, responseInfo);
  }
  else if (GetMessageMagic(response) != magic) {
    ThrowInvalidResponse("Unexpected response message type", requestInfo, responseInfo, ", but got " + DescribeMessageMagic(response));
  }
}

ServerProxy::ServerProxy(std::shared_ptr<messaging::ServerConnection> untyped, const MessageSigner& clientMessageSigner) noexcept
  : mUntyped(std::move(untyped)), mClientMessageSigner(clientMessageSigner) {
  assert(mUntyped != nullptr);
}

rxcpp::observable<ConnectionStatus> ServerProxy::connectionStatus() const {
  return mUntyped->connectionStatus();
}

rxcpp::observable<FakeVoid> ServerProxy::shutdown() {
  return mUntyped->shutdown();
}

rxcpp::observable<VersionResponse> ServerProxy::requestVersion() const {
  return this->sendRequest<VersionResponse>(VersionRequest())
    .op(RxGetOne());
}

rxcpp::observable<MetricsResponse> ServerProxy::requestMetrics() const {
  return this->sendRequest<MetricsResponse>(this->sign(MetricsRequest()))
    .op(RxGetOne());
}

rxcpp::observable<ChecksumChainNamesResponse> ServerProxy::requestChecksumChainNames() const {
  return this->sendRequest<ChecksumChainNamesResponse>(this->sign(ChecksumChainNamesRequest()))
    .op(RxGetOne());
}

rxcpp::observable<ChecksumChainResponse> ServerProxy::requestChecksumChain(ChecksumChainRequest request) const {
  return this->sendRequest<ChecksumChainResponse>(this->sign(std::move(request)))
    .op(RxGetOne());
}

}
