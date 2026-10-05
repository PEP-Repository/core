#pragma once

#include <pep/networking/ExponentialBackoff.hpp>
#include <pep/networking/Node.hpp>
#include <pep/utils/Shared.hpp>

namespace pep::networking {

class Client : public SharedConstructor<Client>, public Node {
  friend class SharedConstructor<Client>;

public:
  using ReconnectParameters = ExponentialBackoff::Parameters;

private:
  class Connection;

  boost::asio::io_context& ioContext_;
  std::shared_ptr<Connection> connection_;
  EventSubscription initialConnectivity_;
  std::optional<ReconnectParameters> reconnectParameters_;

  explicit Client(const Protocol::ClientParameters& parameters, std::optional<ReconnectParameters> reconnectParameters = ReconnectParameters());

  void establishConnection() override;

public:
  ~Client() noexcept override { Client::shutdown(); } // Explicitly scoped to avoid dynamic dispatch
  void shutdown() override;
};

}
