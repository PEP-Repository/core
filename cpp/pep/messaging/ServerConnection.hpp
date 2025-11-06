#pragma once

#include <pep/messaging/ConnectionStatus.hpp>
#include <pep/messaging/Node.hpp>
#include <pep/networking/EndPoint.hpp>
#include <pep/async/FakeVoid.hpp>
#include <pep/async/WaitGroup.hpp>
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
   * @brief Creates a new instance.
   * @param io_context The I/O context associated with the connection.
   * @param endPoint The endpoint at which the server lives.
   * @param caCertFilepath The path to the file containing the (PEM-encoded) CA certificate.
   * @return A freshly created ServerConnection instance.
   */
  static std::shared_ptr<ServerConnection> Create(std::shared_ptr<boost::asio::io_context> io_context, const EndPoint& endPoint, const std::filesystem::path& caCertFilepath);

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
};

}
