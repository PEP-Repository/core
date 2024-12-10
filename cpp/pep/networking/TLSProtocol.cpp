#include <pep/networking/TLSProtocol.hpp>
#include <pep/utils/Log.hpp>
#include <pep/networking/SystemRootCAs.hpp>
#include <pep/async/CreateObservable.hpp>

#include <boost/bind/bind.hpp>

namespace pep {

static const std::string LOG_TAG ("NetioTLS");

TLSProtocol::Parameters::Parameters() {
  TrustSystemRootCAs(*context);
}

std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> TLSProtocol::createSocket() const {
  return std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(*getIOservice(), *mContext);
}

std::shared_ptr<boost::asio::io_service> TLSProtocol::getIOservice() const {
  auto result = mIOservice.lock();
  if (!result) {
    throw std::runtime_error("I/O service is no longer available");
  }
  return result;
}


TLSProtocol::Connection::Connection(std::shared_ptr<TLSProtocol> protocol)
  : mProtocol(protocol), socket(protocol->createSocket()) {
}

boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::lowest_layer_type& TLSProtocol::Connection::getSocket() {
  return socket->lowest_layer();
}

rxcpp::observable<ConnectionStatus> TLSProtocol::Connection::connectionStatus() {
  return connectionSubject.get_observable();
};

void TLSProtocol::Connection::startHandshake(boost::asio::ssl::stream_base::handshake_type type) {
  mState = TLSProtocol::Connection::HANDSHAKE;
  socket->async_handshake(type, boost::bind(&TLSProtocol::Connection::handleHandshake, shared_from_this(), boost::asio::placeholders::error));
}

void TLSProtocol::Connection::handleHandshake(const boost::system::error_code& error) {
  if (!error) {
    LOG(LOG_TAG, debug) << "handleHandshake with " << describe()
                        << " successful";
    mState = TLSProtocol::Connection::HANDSHAKE_DONE;

    onHandshakeSuccess();
  } else {
    if (error.category() == boost::asio::error::ssl_category) {
      int code = ERR_GET_REASON(static_cast<decltype(ERR_get_error())>(error.value()));
      LOG(LOG_TAG, warning) << "handleHandshake error with " << describe()
                            << ": OPENSSL error code: " << code << " " << error.message();
    } else {
      LOG(LOG_TAG, warning) << "handleHandshake error with " << describe()
                            << ": " << error << " " << error.message();
    }
    onConnectFailed(error);
    //TODO Cleanup
  }
}

/*virtual*/ void TLSProtocol::Connection::onConnectFailed(const boost::system::error_code& error) {
  LOG(LOG_TAG, severity_level::debug) << "TLSProtocol::Connection::onConnectFailed with "
                                      << describe() << "; error: " << error << "(" << error.message() << ")";
  if(mState != TLSProtocol::Connection::SHUTDOWN)
    mState = TLSProtocol::Connection::FAILED;
  ConnectionStatus status = { false, error, 0, 0 };
  connectionSubscriber.on_next(status);
}

/*virtual*/ void TLSProtocol::Connection::onHandshakeSuccess() {
  onConnectSuccess();
}

/*virtual*/ void TLSProtocol::Connection::onConnectSuccess() {
  mState = TLSProtocol::Connection::CONNECTED;
  auto status = ConnectionStatus{ true, {}, 0, 0 };
  connectionSubscriber.on_next(status);
}

rxcpp::observable<FakeVoid> TLSProtocol::Connection::shutdown() {
  auto that = shared_from_this();
  this->mState = TLSProtocol::Connection::SHUTDOWN;

  return CreateObservable<FakeVoid>(
      [that](rxcpp::subscriber<FakeVoid> subscriber) {
        that->socket->lowest_layer().cancel();
        that->socket->async_shutdown(
          [socket = that->socket, subscriber, that](boost::system::error_code error) {
            if (error && error.category() == boost::asio::error::ssl_category && error != boost::asio::error::eof) {
              subscriber.on_error(std::make_exception_ptr(error));
              return;
            }
            // TODO: raise hell if we get an error whose .category() is not boost::asio::error::ssl_category. We don't want to pretend that everything is OK!
            socket->lowest_layer().close();
            subscriber.on_next(pep::FakeVoid());
            subscriber.on_completed();

        });
  });
}

}
