#pragma once

#include <pep/server/Server.hpp>
#include <pep/messaging/Node.hpp>

namespace pep {

/*!
* \brief A pep::Server that accepts network connections using its own I/O context.
*/
class NetworkedServer {
private:
  std::shared_ptr<boost::asio::io_context> ioContext_;
  std::shared_ptr<Server> server_;
  std::shared_ptr<messaging::Node> network_;

  NetworkedServer(std::shared_ptr<boost::asio::io_context> ioContext, std::shared_ptr<Server> server, const X509RootCertificates& rootCas, const Configuration& config);

public:
  NetworkedServer(NetworkedServer&&) = default;

  NetworkedServer(const NetworkedServer&) = delete;
  NetworkedServer& operator=(const NetworkedServer&) = delete;
  NetworkedServer& operator=(NetworkedServer&&) = delete;

  /*!
  * \brief Factory function: creates a NetworkedServer instance.
  * \tparam TServer Type type of server that the instance will host. Must inherit from pep::Server.
  * \param config The configuration for the server.
  * \return A NetworkedServer instance hosting the specified type of server.
  */
  template <std::derived_from<Server> TServer>
  requires (std::derived_from<typename TServer::Parameters, Server::Parameters>)
  static NetworkedServer Make(const Configuration& config) {
    auto ioContext = std::make_shared<boost::asio::io_context>();  // TODO: make on the stack instead
    auto parameters = std::make_shared<typename TServer::Parameters>(ioContext, config); // TODO: make on the stack instead
    auto server = std::make_shared<TServer>(parameters);
    return NetworkedServer(ioContext, server, *parameters->getRootCAs(), config);
  }

  /// \copydoc Server::describe
  std::string describe() const { return server_->describe(); }

  /*!
  * \brief Makes the server accept incoming network connections and handle associated requests.
  * \remark Blocks until the "stop" method is called.
  */
  void start();

  /*!
  * \brief Stops (the I/O context associated with) the server, causing it to no longer accept incoming network connections or requests.
  */
  void stop();
};

}
