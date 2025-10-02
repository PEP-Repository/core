#pragma once

#include <pep/messaging/ServerConnection.hpp>
#include <pep/server/MonitoringMessages.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {

class TypedClient {
private:
  std::shared_ptr<messaging::ServerConnection> mUntyped;
  std::shared_ptr<const X509Identity> mSigningIdentity;

protected:
  const std::shared_ptr<messaging::ServerConnection>& untyped() const noexcept { return mUntyped; }

  template <typename T>
  Signed<T> sign(T message) const {
    return Signed(std::move(message), *mSigningIdentity);
  }

  template <typename TResponse, typename TRequest>
  rxcpp::observable<TResponse> requestSingleResponse(TRequest request) const {
    return mUntyped->sendRequest<TResponse>(std::move(request));
  }

  template <typename TResponse>
  rxcpp::observable<TResponse> ping(std::function<PingResponse(const TResponse& rawResponse)> getPlainResponse) const {
    return mUntyped->ping<TResponse>(std::move(getPlainResponse));
  }

public:
  TypedClient(std::shared_ptr<messaging::ServerConnection> untyped, std::shared_ptr<const X509Identity> signingIdentity) noexcept;
  TypedClient(const TypedClient&) = delete;
  TypedClient& operator=(const TypedClient&) = delete;

  rxcpp::observable<ConnectionStatus> connectionStatus() const;
  rxcpp::observable<FakeVoid> shutdown();

  rxcpp::observable<VersionResponse> requestVersion() const;
  rxcpp::observable<MetricsResponse> requestMetrics() const;
  rxcpp::observable<ChecksumChainNamesResponse> requestChecksumChainNames() const;
  rxcpp::observable<ChecksumChainResponse> requestChecksumChain(ChecksumChainRequest request) const;
};

}
