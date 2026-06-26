#include <pep/networking/Client.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <sstream>

namespace pep::networking {

class Client::Connection : public networking::Connection, public std::enable_shared_from_this<Connection> {
  friend class Client;

private:
  std::weak_ptr<Client> client_;
  std::optional<ExponentialBackoff> backoff_;
  bool reconnect_ = false;

  const Event<Connection, const Attempt::Result&> onConnectionAttempt; // TODO: consolidate with onConnectivityChange

  void establish();
  bool shouldReconnect() const noexcept;
  std::optional<ExponentialBackoff::Timeout> reconnect();

public:
  explicit Connection(std::weak_ptr<Client> client, boost::asio::io_context& ioContext, const std::optional<ReconnectParameters>& reconnectParameters);
  ~Connection() noexcept override;

  void close() override;
};


Client::Client(const Protocol::ClientParameters& parameters, std::optional<ReconnectParameters> reconnectParameters)
  : Node(parameters.createComponent()), ioContext_(parameters.ioContext()), reconnectParameters_(reconnectParameters) {
}

void Client::establishConnection() {
  assert(connection_ == nullptr);

  auto weak = WeakFrom(*this);
  connection_ = std::make_shared<Connection>(weak, ioContext_, reconnectParameters_);
  initialConnectivity_ = connection_->onConnectionAttempt.subscribe([weak = std::weak_ptr<Client>(weak)](const Connection::Attempt::Result& result) {
    auto self = weak.lock();
    if (self != nullptr) {
      if (result) {
        self->initialConnectivity_.cancel();
      }
      self->handleConnectionAttempt(result);
    }
    });

  connection_->establish();
}

void Client::Connection::establish() {
  reconnect_ = true;
  this->setConnectivityStatus(ConnectivityStatus::Connecting);

  auto client = client_.lock();
  if (client == nullptr || !client->isRunning()) {
    return this->close();
  }
  assert(client->connection_.get() == this);

  client->openSocket([weak = WeakFrom(*this)](const SocketConnectionAttempt::Result& socketResult) {
    auto self = weak.lock();
    if (self == nullptr) {
      if (socketResult) {
        (*socketResult)->close();
      }
      return;
    }

    auto client = self->client_.lock();
    if (client == nullptr || !client->isRunning()) {
      self->onConnectionAttempt.notify(Attempt::Result::Failure(std::make_exception_ptr(std::runtime_error("Client was shut down"))));
      self->close();
      return;
    }

    if (!socketResult) {
      // Retry establishing the connection
      std::ostringstream message;
      message << "Could not establish connection for " << client->describe()
        << ": " << GetExceptionMessage(socketResult.exception()) << '.';

      auto latency = self->reconnect();
      if (latency.has_value()) {
        message << " Retrying in " << duration_cast<std::chrono::milliseconds>(*latency);
      }

      PEP_LOG("Networking client", Severity::Warning) << message.str();
      self->onConnectionAttempt.notify(Attempt::Result::Failure(socketResult.exception()));
      return;
    }

    // Update own state
    if (self->backoff_.has_value()) {
      self->backoff_->success();
    }
    self->setSocket(*socketResult, [weak](const ConnectivityChange& socketConnectivityChange) {
      auto self = weak.lock();
      if (self != nullptr && socketConnectivityChange.updated == ConnectivityStatus::Disconnecting) {
        self->reconnect();
      }
      });
    assert(self->isConnected());

    // Notify external listener
    self->onConnectionAttempt.notify(Connection::Attempt::Result::Success(self));
    });
}

bool Client::Connection::shouldReconnect() const noexcept {
  auto client = client_.lock();
  return client != nullptr && client->isRunning() // Don't reconnect if the client has been shut down...
    && reconnect_ && backoff_.has_value(); // ... or if we've been instructed not to
}

std::optional<ExponentialBackoff::Timeout> Client::Connection::reconnect() {
  this->discardSocket();

  if (!this->shouldReconnect()) {
    return std::nullopt;
  }

  assert(this->status() < ConnectivityStatus::Disconnecting);
  this->setConnectivityStatus(ConnectivityStatus::Reconnecting);
  if (!this->shouldReconnect()) { // Client may have shut down as a result of our status update
    this->setConnectivityStatus(ConnectivityStatus::Disconnecting);
    return std::nullopt;
  }

  return backoff_->retry([weak = WeakFrom(*this)](const boost::system::error_code& error) {
    if (error != boost::asio::error::operation_aborted) {
      auto self = weak.lock();
      if (self != nullptr && self->shouldReconnect()) {
        self->establish();
      }
    }
    });
}

void Client::shutdown() {
  if (this->status() != Status::Uninitialized && this->status() < Status::Finalizing) {
    this->setStatus(Status::Finalizing);
  }
  if (connection_ != nullptr) {
    connection_->reconnect_ = false;
    connection_->close();
  }
  Node::shutdown();
}

Client::Connection::Connection(std::weak_ptr<Client> client, boost::asio::io_context& ioContext, const std::optional<ReconnectParameters>& reconnectParameters)
  : client_(client) {
  if (reconnectParameters.has_value()) {
    backoff_ = ExponentialBackoff(ioContext, *reconnectParameters);
  }
}

Client::Connection::~Connection() noexcept {
  reconnect_ = false;
  Connection::close(); // Explicitly scoped to avoid dynamic dispatch
}

void Client::Connection::close() {
  if (reconnect_) {
    this->reconnect();
  }
  else {
    if (backoff_.has_value()) {
      backoff_->stop();
    }
    networking::Connection::close();
  }
}

}
