#pragma once

#include <pep/core-client/CoreClient.hpp>
#include <pep/networking/TLSMessageClient.hpp>
#include <pep/networking/NetworkingSerializers.hpp>
#include <utility>

namespace pep {

/*!
  * \brief Network connectivity implementation for the "CoreClient" class.
  */
class CoreClient::ServerConnection {
private:
  std::shared_ptr<TLSMessageClient::Connection> mImplementor;

private:
  explicit ServerConnection(std::shared_ptr<TLSMessageClient::Connection> implementor) noexcept;

public:
  static std::shared_ptr<ServerConnection> TryCreate(std::shared_ptr<boost::asio::io_service> io_service, const EndPoint& endPoint, const std::filesystem::path& caCertFilepath);

  rxcpp::observable<ConnectionStatus> connectionStatus();
  rxcpp::observable<std::string> sendRequest(std::shared_ptr<std::string> message, std::optional<rxcpp::observable<rxcpp::observable<std::shared_ptr<std::string>>>> chunkGenerator = {});
  rxcpp::observable<FakeVoid> shutdown();

  template <typename response_type, typename request_type>
  rxcpp::observable<response_type> sendRequest(request_type&& request) {
    return mImplementor->sendRequest<response_type>(std::forward<request_type>(request));
  }

  template <typename TResponse>
  rxcpp::observable<TResponse> ping(std::function<PingResponse(const TResponse& rawResponse)> getPlainResponse) {
    uint64_t id;
    RandomBytes(reinterpret_cast<uint8_t*>(&id), sizeof(id));

    return this->sendRequest<TResponse>(PingRequest(id))
      .map([getPlainResponse, id](const TResponse& rawResponse) {
      auto response = getPlainResponse(rawResponse);
      if (response.mId != id) {
        throw std::runtime_error("Received ping response with incorrect ID");
      }
      return rawResponse;
        });
  }
};

}
