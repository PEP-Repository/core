#pragma once

#include <pep/messaging/ConnectionStatus.hpp>
#include <pep/messaging/Node.hpp>
#include <pep/networking/EndPoint.hpp>
#include <pep/async/FakeVoid.hpp>
#include <pep/async/WaitGroup.hpp>
#include <pep/utils/Random.hpp>
#include <utility>

namespace pep::messaging {

/**
 * @brief A (client) connection to a PEP server.
 */
class ServerConnection : public std::enable_shared_from_this<ServerConnection> {
private:
  std::shared_ptr<Node> mNode;
  std::shared_ptr<Connection> mConnection;
  EventSubscription mConnectionStatusSubscription;
  std::shared_ptr<WaitGroup> mWaitGroup;
  std::optional<WaitGroup::Action> mConnecting;
  rxcpp::subjects::behavior<ConnectionStatus> mStatus{ {false, boost::system::errc::make_error_code(boost::system::errc::no_message)} };
  rxcpp::subscriber<ConnectionStatus> mStatusSubscriber = mStatus.get_subscriber();

  explicit ServerConnection(std::shared_ptr<Node> node) noexcept;

  void handleConnectivityChange(const LifeCycler::StatusChange& change);
  void handleConnectivityChange(const Connection::Attempt::Result& result);
  void handleConnectivityError(std::exception_ptr error);
  void handleConnectivityEnd();

  void onConnected(std::shared_ptr<Connection> connection);
  void onDisconnected();

  void finalize();

  template <typename TResponse>
  using RequestFunction = std::function<rxcpp::observable<TResponse>(std::shared_ptr<Connection>)>;

  template <typename TResponse>
  rxcpp::observable<TResponse> whenConnected(RequestFunction<TResponse> request) {
    //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) Function leak seems like a false positive as long as WaitGroup finishes (TODO we should probably support cancellation)
    return mWaitGroup->delayObservable<TResponse>([weak = WeakFrom(*this), request]() -> rxcpp::observable<TResponse> {
      auto self = weak.lock();
      if (self == nullptr || self->mNode == nullptr) {
        return rxcpp::observable<>::error<TResponse>(std::runtime_error("Server connection was lost or closed"));
      }
      assert(self->mConnection != nullptr);
      return request(self->mConnection);
      });
  }

public:
  /**
   * @brief Creates a new instance if the endpoint's host name is set.
   * @param io_context The I/O context associated with the connection.
   * @param endPoint The endpoint at which the server lives. If the host name is not set, no connection will be created (and a NULL pointer returned).
   * @param caCertFilepath The path to the file containing the (PEM-encoded) CA certificate.
   * @return A freshly created ServerConnection instance, or NULL if the endPoint didn't contain a host name.
   */
  static std::shared_ptr<ServerConnection> TryCreate(std::shared_ptr<boost::asio::io_context> io_context, const EndPoint& endPoint, const std::filesystem::path& caCertFilepath);

  /**
   * @brief An observable representing the connection's current status.
   * @return An observable representing the connection's current status.
   */
  rxcpp::observable<ConnectionStatus> connectionStatus();

  /**
   * @brief Sends a serialized request to the server.
   * @param message The (serialized) request to send.
   * @param tail Any followup messages associated with the request. Pass std::nullopt if the request consists of a single (head) message.
   * @return An observable that emits the server's serialized response messages.
   */
  rxcpp::observable<std::string> sendRequest(std::shared_ptr<std::string> message, std::optional<messaging::MessageBatches> tail = {});

  /**
   * @brief Shuts down the connection.
   * @return An observable that finishes when shutdown is complete.
   */
  rxcpp::observable<FakeVoid> shutdown();

  /**
   * @brief Sends a request to the server, returning the server's (single) response message.
   * @param request The request to send.
   * @return An observable that emits the server's response.
   * @remark Usable only for requests (without tail messages) for which the server returns a single response message.
   */
  template <typename response_type, typename request_type>
  rxcpp::observable<response_type> sendRequest(request_type&& request) {
    return this->whenConnected<response_type>([request = std::forward<request_type>(request)](std::shared_ptr<Connection> connection) {
      return connection->sendRequest<response_type>(request);
      });
  }

  /**
   * @brief Sends a PingRequest to the server, returning the server's response ("pong") message.
   * @param getPlainResponse A function that produces a (plain) PingResponse instance from the server's raw response message (e.g. a SignedPingResponse).
   * @return An observable that emits the server's response.
   */
  template <typename TResponse>
  rxcpp::observable<TResponse> ping(std::function<PingResponse(const TResponse& rawResponse)> getPlainResponse) {
    uint64_t id;
    RandomBytes(reinterpret_cast<uint8_t*>(&id), sizeof(id));

    return this->whenConnected<TResponse>([id, getPlainResponse](std::shared_ptr<Connection> connection) -> rxcpp::observable<TResponse> {
      return connection->sendRequest<TResponse>(PingRequest(id))
        .map([getPlainResponse, id](const TResponse& rawResponse) {
        auto response = getPlainResponse(rawResponse);
        if (response.mId != id) {
          throw std::runtime_error("Received ping response with incorrect ID");
        }
        return rawResponse;
          });
      });
  }
};

}
