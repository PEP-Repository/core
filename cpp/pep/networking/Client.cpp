#include <pep/networking/Client.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <sstream>

namespace pep::networking {

class Client::Connection : public networking::Connection, public std::enable_shared_from_this<Connection> {
  friend class Client;

private:
  std::weak_ptr<Client> mClient;
  std::optional<ExponentialBackoff> mBackoff;
  bool mReconnect = false;

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
  : Node(parameters.createComponent()), mIoContext(parameters.ioContext()), mReconnectParameters(reconnectParameters) {
}

void Client::establishConnection() {
  assert(mConnection == nullptr);

  auto weak = WeakFrom(*this);
  mConnection = std::make_shared<Connection>(weak, mIoContext, mReconnectParameters);
  mInitialConnectivity = mConnection->onConnectionAttempt.subscribe([weak = std::weak_ptr<Client>(weak)](const Connection::Attempt::Result& result) {
    auto self = weak.lock();
    if (self != nullptr) {
      if (result) {
        self->mInitialConnectivity.cancel();
      }
      self->handleConnectionAttempt(result);
    }
    });

  mConnection->establish();
}

void Client::Connection::establish() {
  mReconnect = true;
  this->setConnectivityStatus(ConnectivityStatus::connecting);

  auto client = mClient.lock();
  if (client == nullptr || !client->isRunning()) {
    return this->close();
  }
  assert(client->mConnection.get() == this);

  client->openSocket([weak = WeakFrom(*this)](const SocketConnectionAttempt::Result& socketResult) {
    auto self = weak.lock();
    if (self == nullptr) {
      if (socketResult) {
        (*socketResult)->close();
      }
      return;
    }

    auto client = self->mClient.lock();
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

      LOG("Networking client", warning) << message.str();
      self->onConnectionAttempt.notify(Attempt::Result::Failure(socketResult.exception()));
      return;
    }

    // Update own state
    if (self->mBackoff.has_value()) {
      self->mBackoff->success();
    }
    self->setSocket(*socketResult, [weak](const ConnectivityChange& socketConnectivityChange) {
      auto self = weak.lock();
      if (self != nullptr && socketConnectivityChange.updated == ConnectivityStatus::disconnecting) {
        self->reconnect();
      }
      });
    assert(self->isConnected());

    // Notify external listener
    self->onConnectionAttempt.notify(Connection::Attempt::Result::Success(self));
    });
}

bool Client::Connection::shouldReconnect() const noexcept {
  auto client = mClient.lock();
  return client != nullptr && client->isRunning() // Don't reconnect if the client has been shut down...
    && mReconnect && mBackoff.has_value(); // ... or if we've been instructed not to
}

std::optional<ExponentialBackoff::Timeout> Client::Connection::reconnect() {
  this->discardSocket();

  if (!this->shouldReconnect()) {
    return std::nullopt;
  }

  assert(this->status() < ConnectivityStatus::disconnecting);
  this->setConnectivityStatus(ConnectivityStatus::reconnecting);
  if (!this->shouldReconnect()) { // Client may have shut down as a result of our status update
    this->setConnectivityStatus(ConnectivityStatus::disconnecting);
    return std::nullopt;
  }

  return mBackoff->retry([weak = WeakFrom(*this)](const boost::system::error_code& error) {
    if (error != boost::asio::error::operation_aborted) {
      auto self = weak.lock();
      if (self != nullptr && self->shouldReconnect()) {
        self->establish();
      }
    }
    });
}

void Client::shutdown() {
  if (this->status() != Status::uninitialized && this->status() < Status::finalizing) {
    this->setStatus(Status::finalizing);
  }
  if (mConnection != nullptr) {
    mConnection->mReconnect = false;
    mConnection->close();
  }
  Node::shutdown();
}

Client::Connection::Connection(std::weak_ptr<Client> client, boost::asio::io_context& ioContext, const std::optional<ReconnectParameters>& reconnectParameters)
  : mClient(client) {
  if (reconnectParameters.has_value()) {
    mBackoff = ExponentialBackoff(ioContext, *reconnectParameters);
  }
}

Client::Connection::~Connection() noexcept {
  mReconnect = false;
  Connection::close(); // Explicitly scoped to avoid dynamic dispatch
}

void Client::Connection::close() {
  if (mReconnect) {
    this->reconnect();
  }
  else {
    if (mBackoff.has_value()) {
      mBackoff->stop();
    }
    networking::Connection::close();
  }
}

}
