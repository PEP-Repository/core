#include <pep/server/NetworkedServer.hpp>
#include <pep/messaging/BinaryProtocol.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Exceptions.hpp>

namespace pep {

namespace {

const std::string LOG_TAG = "Networked server";

std::shared_ptr<messaging::Node> CreateNetworkingNode(boost::asio::io_context& ioContext, std::shared_ptr<Server> server, const Configuration& config) {
  auto port = config.get<uint16_t>("ListenPort");
  auto identity = X509IdentityFiles::FromConfig(config, "TLS");
  auto binaryParameters = pep::messaging::BinaryProtocol::CreateServerParameters(ioContext, port, std::move(identity));
  return messaging::Node::Create(*binaryParameters, *server);
}

// Helper class that (1) keeps a connection (and itself) alive until the connection is closed and (2) forwards the connection's uncaught read exceptions to a Server
class ConnectionKeeper {
private:
  std::shared_ptr<messaging::Connection> mConnection;
  EventSubscription mStatusChange;
  EventSubscription mUncaughtReadException;

  ConnectionKeeper(std::shared_ptr<messaging::Connection> connection, std::shared_ptr<Server> server)
    : mConnection(std::move(connection)) {
    mUncaughtReadException = mConnection->onUncaughtReadException.subscribe([server](std::exception_ptr exception) {
      server->registerUncaughtReadException(exception);
      });
  }

public:
  static std::shared_ptr<ConnectionKeeper> Create(std::shared_ptr<messaging::Connection> connection, std::shared_ptr<Server> server) {
    std::shared_ptr<ConnectionKeeper> result(new ConnectionKeeper(connection, server));

    // Let the ConnectionKeeper keep itself alive until the connection gets closed
    result->mStatusChange = result->mConnection->onStatusChange.subscribe([result /* keep the ConnectionKeeper instance alive */](const LifeCycler::StatusChange& change) {
      if (change.updated >= LifeCycler::Status::finalizing) {
        // Discard this lambda, getting rid of the shared_ptr to the ConnectionKeeper, allowing the instance and its mConnection to be destroyed.
        result->mStatusChange.cancel();
      }
      });

    return result;
  }
};

}

NetworkedServer::NetworkedServer(std::shared_ptr<boost::asio::io_context> ioContext, std::shared_ptr<Server> server, const Configuration& config)
  : mIoContext(ioContext), mServer(std::move(server)), mNetwork(CreateNetworkingNode(*ioContext, mServer, config)) {
}

void NetworkedServer::start() {
  mNetwork->start()
    .subscribe(
      [this](const messaging::Connection::Attempt::Result& result) {
        if (!result) {
          LOG(LOG_TAG, severity_level::info) << "Incoming connection to " << mServer->describe() << " could not be established: " << GetExceptionMessage(result.exception());
        } else {
          ConnectionKeeper::Create(*result, mServer);
        }
      },
      [](std::exception_ptr error) {
        LOG(LOG_TAG, severity_level::error) << "Server networking failed due to " << GetExceptionMessage(error);
      },
      []() {
        assert(false); // Should never occur because we don't invoke mNetwork.shutdown()
      });

  mIoContext->run();
}

void NetworkedServer::stop() {
  mIoContext->stop();
}

}
