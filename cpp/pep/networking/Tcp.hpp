#pragma once

#include <pep/networking/TcpBasedProtocol.hpp>

namespace pep::networking {

/*!
* \brief The TCP networking protocol.
*/
class Tcp : public TcpBasedProtocolImplementor<Tcp> {
private:
  std::shared_ptr<Socket> createSocket(boost::asio::io_context& context) const;

protected:
  std::shared_ptr<TcpBasedProtocol::Socket> createSocket(TcpBasedProtocol::ClientComponent& component) const override {
    return this->createSocket(component.ioContext());
  }

  std::shared_ptr<TcpBasedProtocol::Socket> createSocket(TcpBasedProtocol::ServerComponent& component) const override {
    return this->createSocket(component.ioContext());
  }

  std::shared_ptr<Protocol::ClientParameters> createClientParameters(const Protocol::ServerComponent& server) const override;

public:
  /// \copydoc Protocol::name
  std::string name() const override { return "tcp"; }
};

}
