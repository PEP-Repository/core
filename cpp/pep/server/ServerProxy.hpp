#pragma once

#include <pep/messaging/ServerConnection.hpp>
#include <pep/messaging/Tail.hpp>
#include <pep/server/MonitoringMessages.hpp>

namespace pep {

class ServerProxy {
private:
  std::shared_ptr<messaging::ServerConnection> mUntyped;
  const MessageSigner& mClientMessageSigner;

  static void ValidateResponse(MessageMagic magic, std::string_view response, const std::type_info& responseInfo, const std::type_info& requestInfo);

  template <typename TResponse, typename TRequest>
  static TResponse DeserializeResponse(std::string_view serialized) {
    static_assert(!std::is_same_v<TResponse, Error>, "Ambiguous: should Error instance be raised or deserialized?");
    ValidateResponse(MessageMagician<TResponse>::GetMagic(), serialized, typeid(TResponse), typeid(TRequest));
    return Serialization::FromString<TResponse>(serialized);
  }

protected:
  template <typename T>
  Signed<T> sign(T message) const {
    return mClientMessageSigner.sign(std::move(message));
  }

  template <typename TResponse, typename TRequest>
  rxcpp::observable<TResponse> sendRequest(TRequest request) const {
    return mUntyped->sendRequest(MakeSharedCopy(Serialization::ToString(std::move(request))))
      .map(&DeserializeResponse<TResponse, TRequest>);
  }

  template <typename TResponse, typename TRequest>
  rxcpp::observable<TResponse> sendRequest(TRequest request, messaging::MessageBatches tail) const {
    // TODO: ensure that tail is long rather than wide: see comment for MessageBatches
    return mUntyped->sendRequest(MakeSharedCopy(Serialization::ToString(std::move(request))), tail)
      .map(&DeserializeResponse<TResponse, TRequest>);
  }

  template <typename TResponse, typename TRequest, typename TTail>
  rxcpp::observable<TResponse> sendRequest(TRequest request, messaging::Tail<TTail> tail) const {
    auto batches = tail
      .map([](const messaging::TailSegment<TTail>& segment) -> messaging::MessageSequence {
      return segment
        .map([](TTail single) {
        return MakeSharedCopy(Serialization::ToString(std::move(single)));
          });
        });

    return this->sendRequest<TResponse>(std::move(request), std::move(batches));
  }

public:
  /// @brief Constructor
  /// @param untyped The ServerConnection that can exchange messages with the proxied server
  /// @param clientMessageSigner The instance that will sign messages sent to the server.
  /// @remark Caller must ensure that the MessageSigner outlives the ServerProxy
  ServerProxy(std::shared_ptr<messaging::ServerConnection> untyped, const MessageSigner& clientMessageSigner);
  virtual ~ServerProxy() noexcept = default;
  ServerProxy(const ServerProxy&) = delete;
  ServerProxy& operator=(const ServerProxy&) = delete;

  rxcpp::observable<ConnectionStatus> connectionStatus() const;
  rxcpp::observable<FakeVoid> shutdown();

  virtual rxcpp::observable<PingResponse> requestPing() const = 0;

  rxcpp::observable<VersionResponse> requestVersion() const;
  rxcpp::observable<MetricsResponse> requestMetrics() const;
  rxcpp::observable<ChecksumChainNamesResponse> requestChecksumChainNames() const;
  rxcpp::observable<ChecksumChainResponse> requestChecksumChain(ChecksumChainRequest request) const;
};

}
