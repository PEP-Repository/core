#include <pep/core-client/CoreClient.ServerConnection.hpp>

namespace pep {

CoreClient::ServerConnection::ServerConnection(std::shared_ptr<TLSMessageClient::Connection> implementor) noexcept
  : mImplementor(implementor) {
  assert(mImplementor != nullptr);
}

std::shared_ptr<CoreClient::ServerConnection> CoreClient::ServerConnection::TryCreate
(std::shared_ptr<boost::asio::io_context> io_context, const EndPoint& endPoint, const std::filesystem::path& caCertFilepath) {
  if (!endPoint.hostname.empty()) {
    auto parameters = std::make_shared<TLSMessageClient::Parameters>();
    parameters->setEndPoint(endPoint);
    parameters->setIoContext(io_context);
    parameters->setCaCertFilepath(caCertFilepath);
    auto implementor = CreateTLSClientConnection<TLSMessageClient>(parameters);
    return std::shared_ptr<ServerConnection>(new ServerConnection(implementor));
  }
  return nullptr;
}

rxcpp::observable<ConnectionStatus> CoreClient::ServerConnection::connectionStatus() {
  return mImplementor->connectionStatus();
}

rxcpp::observable<std::string> CoreClient::ServerConnection::sendRequest(std::shared_ptr<std::string> message, std::optional<MessageBatches> tail) {
  return mImplementor->sendRequest(message, tail);
}

rxcpp::observable<FakeVoid> CoreClient::ServerConnection::shutdown() {
  return mImplementor->shutdown();
}

}
