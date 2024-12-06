#include <pep/networking/TLSMessageClient.hpp>

#include <boost/bind/bind.hpp>

namespace pep {

/*virtual*/ void TLSMessageClient::Connection::onConnectFailed(const boost::system::error_code& error) {
  TLSClient<TLSMessageProtocol>::Connection::onConnectFailed(error); // need to go first because outgoing messages are cleared
  this->resendOutstandingRequests();
}

}
