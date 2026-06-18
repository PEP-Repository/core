#include <pep/networking/Tcp.hpp>

namespace pep::networking {

namespace {

class TcpSocket : public TcpBasedProtocolImplementor<Tcp>::Socket {
private:
  boost::asio::ip::tcp::socket implementor_;
  StreamSocket mStreamSocket;

protected:
  BasicSocket& basicSocket() override { return implementor_; }
  const BasicSocket& basicSocket() const override { return implementor_; }
  StreamSocket& streamSocket() override { return mStreamSocket; }

  void close() override {
    if (this->status() != ConnectivityStatus::Unconnected && this->status() < ConnectivityStatus::Disconnecting) {
      this->setConnectivityStatus(ConnectivityStatus::Disconnecting);
      implementor_.close();
    }
    this->setConnectivityStatus(ConnectivityStatus::Disconnected);
  }

public:
  explicit TcpSocket(const Tcp& protocol, boost::asio::io_context& ioContext)
    : Socket(protocol, ioContext), implementor_(ioContext), mStreamSocket(implementor_) {}
};

}

std::shared_ptr<Tcp::Socket> Tcp::createSocket(boost::asio::io_context& context) const {
  return std::make_shared<TcpSocket>(*this, context);
}

std::shared_ptr<Protocol::ClientParameters> Tcp::createClientParameters(const Protocol::ServerComponent& server) const {
  auto& downcast = server.downcastFor(*this);
  EndPoint endPoint("localhost", downcast.port());
  return std::make_shared<ClientParameters>(downcast.ioContext(), endPoint);
}

}
