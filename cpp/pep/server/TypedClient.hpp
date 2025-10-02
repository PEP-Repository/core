#pragma once

#include <pep/messaging/ServerConnection.hpp>
#include <pep/server/MonitoringMessages.hpp>
#include <rxcpp/rx-lite.hpp>

namespace pep {

template <typename T>
using TailSegment = rxcpp::observable<T>;

template <typename T>
using MessageTail = rxcpp::observable<TailSegment<T>>;


class TypedClient {
private:
  std::shared_ptr<messaging::ServerConnection> mUntyped;
  const MessageSigner& mMessageSigner;

protected:
  const std::shared_ptr<messaging::ServerConnection>& untyped() const noexcept { return mUntyped; }

  template <typename T>
  Signed<T> sign(T message) const {
    return mMessageSigner.sign(std::move(message));
  }

  template <typename TResponse, typename TRequest>
  rxcpp::observable<TResponse> sendRequest(TRequest request) const {
    return mUntyped->sendRequest(MakeSharedCopy(Serialization::ToString(std::move(request))))
      .map([](std::string serialized) { return Serialization::FromString<TResponse>(std::move(serialized)); });
  }

  template <typename TResponse, typename TRequest, typename TTail>
  rxcpp::observable<TResponse> sendRequest(TRequest request, MessageTail<TTail> tail) const {
    auto batches = tail
      .map([](const TailSegment<TTail>& segment) -> messaging::MessageSequence {
      return segment
        // TODO: ensure that tail is long rather than wide: see comment for MessageBatches
        .map([](TTail single) {
        return MakeSharedCopy(Serialization::ToString(std::move(single)));
          });
        });

    return mUntyped->sendRequest(MakeSharedCopy(Serialization::ToString(std::move(request))), batches)
      .map([](std::string serialized) { return Serialization::FromString<TResponse>(std::move(serialized)); });
  }

  // TODO: remove
  template <typename TResponse>
  rxcpp::observable<TResponse> ping(std::function<PingResponse(const TResponse& rawResponse)> getPlainResponse) const {
    return mUntyped->ping<TResponse>(std::move(getPlainResponse));
  }

public:
  TypedClient(std::shared_ptr<messaging::ServerConnection> untyped, const MessageSigner& messageSigner) noexcept;
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
