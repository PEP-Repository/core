#pragma once

#include <pep/networking/TLSClient.hpp>
#include <pep/networking/TLSMessageProtocol.hpp>

namespace pep {

class TLSMessageClient : public TLSClient<TLSMessageProtocol> {
 public:
  typedef TLSClient<TLSMessageProtocol>::Parameters Parameters;

  class Connection : public TLSClient<TLSMessageProtocol>::Connection {
  protected:
    std::shared_ptr<TLSMessageClient> getClient() const noexcept { return std::static_pointer_cast<TLSMessageClient>(TLSClient<TLSMessageProtocol>::Connection::getClient()); }
    void onConnectFailed(const boost::system::error_code& error) override;
    std::string describe() const override {
      return getEndPoint().Describe();
    }

  public:
    //! Construct a TLSClientMessageConnection from parameters
    Connection(std::shared_ptr<TLSMessageClient> client)
      : TLSClient<TLSMessageProtocol>::Connection(client) {}
    virtual ~Connection() { }
  };

  TLSMessageClient(std::shared_ptr<Parameters> parameters) : TLSClient<TLSMessageProtocol>(parameters) {}
};

}
