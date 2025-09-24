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

  boost::asio::io_context& mIoContext;
  std::shared_ptr<networking::Node> mBinary;
  EventSubscription mBinaryConnectionAttempt;
  RequestHandler* mRequestHandler = nullptr;
  std::optional<rxcpp::subscriber<Connection::Attempt::Result>> mSubscriber;
  std::optional<std::set<IncompatibleRemote>> mIncompatibleRemotes;

  std::vector<std::weak_ptr<networking::Connection>> mExistingConnections;

  Node(boost::asio::io_context& ioContext, std::shared_ptr<networking::Server> binary, RequestHandler& requestHandler); // TODO: extract io_context from Server instead of requiring it to be passed separately
  Node(boost::asio::io_context& ioContext, std::shared_ptr<networking::Client> binary); // TODO: extract io_context from Client instead of requiring it to be passed separately

  Node(const networking::Protocol::ServerParameters& parameters, RequestHandler& requestHandler);
  Node(const networking::Protocol::ClientParameters& parameters, std::optional<networking::Client::ReconnectParameters> reconnectParameters = networking::Client::ReconnectParameters());

  void vetConnectionWith(const std::string& description, const std::string& address, const BinaryVersion& binary, const std::optional<ConfigVersion>& config);

public:
  ~Node() noexcept;

  std::string describe() const;

  rxcpp::observable<Connection::Attempt::Result> start();
  rxcpp::observable<FakeVoid> shutdown();
};

}
