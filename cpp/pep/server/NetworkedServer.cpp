#include <pep/server/NetworkedServer.hpp>
#include <pep/messaging/BinaryProtocol.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Exceptions.hpp>

namespace pep {

namespace {

const std::string LogTag = "Networked server";

std::shared_ptr<messaging::Node> CreateNetworkingNode(boost::asio::io_context& ioContext, std::shared_ptr<Server> server, const X509RootCertificates& rootCas, const Configuration& config) {
  auto port = config.get<uint16_t>("ListenPort");
  auto identity = X509IdentityFiles::FromConfig(config.get_child("TlsIdentity"), rootCas);
  auto binaryParameters = pep::messaging::BinaryProtocol::CreateServerParameters(ioContext, port, std::move(identity));
  return messaging::Node::Create(*binaryParameters, *server);
}

// Helper class that (1) keeps a connection (and itself) alive until the connection is closed and (2) forwards the connection's uncaught read exceptions to a Server
class ConnectionKeeper {
private:
  std::shared_ptr<messaging::Connection> connection_;
  EventSubscription statusChange_;
  EventSubscription uncaughtReadException_;

  ConnectionKeeper(std::shared_ptr<messaging::Connection> connection, std::shared_ptr<Server> server)
    : connection_(std::move(connection)) {
    uncaughtReadException_ = connection_->onUncaughtReadException.subscribe([server](std::exception_ptr exception) {
      server->registerUncaughtReadException(exception);
      });
  }

public:
  static std::shared_ptr<ConnectionKeeper> Create(std::shared_ptr<messaging::Connection> connection, std::shared_ptr<Server> server) {
    std::shared_ptr<ConnectionKeeper> result(new ConnectionKeeper(connection, server));

    // Let the ConnectionKeeper keep itself alive until the connection gets closed
    result->statusChange_ = result->connection_->onStatusChange.subscribe([result /* keep the ConnectionKeeper instance alive */](const LifeCycler::StatusChange& change) {
      if (change.updated >= LifeCycler::Status::Finalizing) {
        // Discard this lambda, getting rid of the shared_ptr to the ConnectionKeeper, allowing the instance and its connection_ to be destroyed.
        result->statusChange_.cancel();
      }
      });

    return result;
  }
};

}

NetworkedServer::NetworkedServer(std::shared_ptr<boost::asio::io_context> ioContext, std::shared_ptr<Server> server, const X509RootCertificates& rootCas, const Configuration& config)
  : ioContext_(ioContext), server_(std::move(server)), network_(CreateNetworkingNode(*ioContext, server_, rootCas, config)) {
}

void NetworkedServer::start() {
  network_->start()
    .subscribe(
      [this](const messaging::Connection::Attempt::Result& result) {
        if (!result) {
          PEP_LOG(LogTag, Severity::Warning) << "Incoming connection to " << server_->describe() << " could not be established: " << GetExceptionMessage(result.exception());
        } else {
          ConnectionKeeper::Create(*result, server_);
        }
      },
      [](std::exception_ptr error) {
        PEP_LOG(LogTag, Severity::Error) << "Server networking failed due to " << GetExceptionMessage(error);
      },
      []() {
        assert(false); // Should never occur because we don't invoke network_.shutdown()
      });

  ioContext_->run();
}

void NetworkedServer::stop() {
  ioContext_->stop();
}

}
