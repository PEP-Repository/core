#pragma once

#include <pep/networking/Connection.hpp>
#include <pep/networking/Protocol.hpp>
#include <unordered_map>

namespace pep::networking {

/**
 * @brief A party that engages in network communications.
 */
class Node : public LifeCycler, public std::enable_shared_from_this<Node> {
private:
  std::shared_ptr<Protocol::NodeComponent> mComponent;
  std::unordered_map<std::shared_ptr<Protocol::Socket>, EventSubscription> mSockets;

protected:
  using SocketConnectionAttempt = Protocol::ConnectionAttempt;

  explicit Node(std::shared_ptr<Protocol::NodeComponent> component) noexcept;

  bool isRunning() const noexcept { return this->status() == Status::initialized; }
  void openSocket(const SocketConnectionAttempt::Handler& onSocketConnection);
  void handleConnectionAttempt(const Connection::Attempt::Result& status) const;
  virtual void establishConnection() = 0;

public:
  ~Node() noexcept override { Node::shutdown(); } // Explicitly scoped to avoid dynamic dispatch

  /**
   * @brief Produces a human-readable description of this networking node.
   * @return A string describing this node.
   */
  std::string describe() const { return mComponent->describe(); }

  /**
   * @brief Makes the node start its networking.
   */
  void start();

  /**
   * @brief Makes the node stop its networking, causing all associated sockets (and hence connections) to be(come) closed.
   */
  virtual void shutdown();

  /**
   * @brief Event that is notified when the node has attempted to establish a network connection.
   */
  const Event<Node, const Connection::Attempt::Result> onConnectionAttempt;
};

}
