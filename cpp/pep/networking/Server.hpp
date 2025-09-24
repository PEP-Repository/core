#pragma once

#include <pep/networking/Node.hpp>
#include <pep/utils/Shared.hpp>

namespace pep::networking {

/**
 * @brief A Node that accepts incoming connections.
 */
class Server : public SharedConstructor<Server>, public Node {
  friend class SharedConstructor<Server>;

private:
  class Connection;

  std::shared_ptr<Protocol::ServerComponent> mComponent;

  explicit Server(std::shared_ptr<Protocol::ServerComponent> component)
    : Node(component), mComponent(component) {
  }

  explicit Server(const Protocol::ServerParameters& parameters)
    : Server(parameters.createComponent()) {
  }

  bool acceptNewClient();

protected:
  void establishConnection() override;

public:
  ~Server() noexcept override { Server::shutdown(); } // Explicitly scoped to avoid dynamic dispatch

  /// \copydoc Node::shutdown
  void shutdown() override;

  /**
   * @brief Creates parameters for a local client to connect to this server.
   * @return A ClientParameters instance with which a Client instance can be created.
   */
  std::shared_ptr<Protocol::ClientParameters> createClientParameters() const;
};

}
