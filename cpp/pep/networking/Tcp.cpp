#include <pep/networking/Tcp.hpp>

namespace pep::networking {

namespace {

class TcpSocket : public TcpBasedProtocolImplementor<Tcp>::Socket {
private:
  boost::asio::ip::tcp::socket mImplementor;
  StreamSocket mStreamSocket;

protected:
  BasicSocket& basicSocket() override { return mImplementor; }
  const BasicSocket& basicSocket() const override { return mImplementor; }
  StreamSocket& streamSocket() override { return mStreamSocket; }

  void close() override {
    if (this->status() != ConnectivityStatus::unconnected && this->status() < ConnectivityStatus::disconnecting) {
      this->setConnectivityStatus(ConnectivityStatus::disconnecting);
      mImplementor.close();
    }
    this->setConnectivityStatus(ConnectivityStatus::disconnected);
  }

public:
  explicit TcpSocket(const Tcp& protocol, boost::asio::io_context& ioContext)
    : Socket(protocol, ioContext), mImplementor(ioContext), mStreamSocket(mImplementor) {}
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
