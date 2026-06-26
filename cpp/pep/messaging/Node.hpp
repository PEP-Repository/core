#pragma once

#include <pep/networking/Client.hpp>
#include <pep/networking/Server.hpp>
#include <pep/messaging/Connection.hpp>

namespace pep::messaging {

class Node : boost::noncopyable, public std::enable_shared_from_this<Node>, public SharedConstructor<Node> {
  friend class SharedConstructor<Node>;
  friend class Connection;

private:
  struct IncompatibleRemote {
    std::string address;
    std::string binary;
    std::string config;

    auto operator<=>(const IncompatibleRemote& rhs) const = default; // Allow instances of this type to be stored in e.g. an std::set<>
  };

  boost::asio::io_context& ioContext_;
  std::optional<networking::Client::ReconnectParameters> reconnectParameters_;
  std::shared_ptr<networking::Node> binary_;
  EventSubscription binaryConnectionAttempt_;
  RequestHandler* requestHandler_ = nullptr;
  std::optional<rxcpp::subscriber<Connection::Attempt::Result>> subscriber_;
  std::optional<std::set<IncompatibleRemote>> incompatibleRemotes_;

  struct ExistingConnection {
    std::weak_ptr<networking::Connection> binary;
    std::shared_ptr<Connection> own;
    EventSubscription establishing;
  };

  std::vector<ExistingConnection> existingConnections_;

  Node(const networking::Protocol::ServerParameters& parameters, RequestHandler& requestHandler);
  Node(const networking::Protocol::ClientParameters& parameters, std::optional<networking::Client::ReconnectParameters> reconnectParameters = networking::Client::ReconnectParameters());

  void vetConnectionWith(const std::string& description, const std::string& address, const BinaryVersion& binary, const std::optional<ConfigVersion>& config);
  void handleConnectionEstablishing(std::shared_ptr<Connection> connection, const LifeCycler::StatusChange& change);

public:
  ~Node() noexcept;

  const std::optional<networking::Client::ReconnectParameters>& reconnectParameters() const noexcept { return reconnectParameters_; }

  std::string describe() const;

  rxcpp::observable<Connection::Attempt::Result> start();
  rxcpp::observable<FakeVoid> shutdown();
};

}
