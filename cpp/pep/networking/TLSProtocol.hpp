#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>
#include <rxcpp/rx-lite.hpp>

#include <ctime>

#include <pep/crypto/X509Certificate.hpp>
#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/networking/ConnectionStatus.hpp>
#include <pep/async/FakeVoid.hpp>

namespace pep {

// TLS implementation
// Based on www.boost.org/doc/libs/1_62_0/doc/html/boost_asio/example/cpp03/ssl/server.cpp

class TLSProtocol {
 public:
  /*!
   * \brief Settings for a TLSConnection
   */
  class Parameters {
   public:
    Parameters();
    virtual inline ~Parameters() {}

    /*!
     * \param io_service The IO service to use in the connection
     */
    void setIOservice(std::shared_ptr<boost::asio::io_service> io_service) {
      this->io_service = io_service;
    }
    //! \return The IO service to use in the connection
    std::shared_ptr<boost::asio::io_service> getIOservice() {
      return io_service;
    }
    //! The context for the connection
    std::shared_ptr<boost::asio::ssl::context> getContext() {
      return context;
    }

    //! Check whether all required fields are set correctly. Throws an exception when something is incorrect.
    virtual void check() {
      if(!io_service) {
        throw std::runtime_error("IOservice is not set");
      }
    }

    Parameters& ensureValid() {
      check();
      return *this;
    }

   private:
    std::shared_ptr<boost::asio::io_service> io_service;
    std::shared_ptr<boost::asio::ssl::context> context = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
  };

  class Connection : public std::enable_shared_from_this<Connection> {
  public:
    enum State {
      UNINITIALIZED,
      HANDSHAKE,
      HANDSHAKE_DONE,
      CONNECTED,
      FAILED,
      SHUTDOWN
    };

    boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::lowest_layer_type& getSocket();

    // to be called after socket is initialised with a proper connection
    void startHandshake(boost::asio::ssl::stream_base::handshake_type type);

  private:
    std::shared_ptr<TLSProtocol> mProtocol;
    rxcpp::subjects::behavior<ConnectionStatus> connectionSubject{ {false, boost::system::errc::make_error_code(boost::system::errc::no_message), 0, 0} };
    rxcpp::subscriber<ConnectionStatus> connectionSubscriber = connectionSubject.get_subscriber();

    // to be called after TLS handshake is completed
    void handleHandshake(const boost::system::error_code& error);

  protected:
    std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> socket;
    State mState = TLSProtocol::Connection::UNINITIALIZED;

    virtual void onHandshakeSuccess();
    virtual void onConnectFailed(const boost::system::error_code& error);
    virtual void onConnectSuccess();

    std::shared_ptr<TLSProtocol> getProtocol() const noexcept { return mProtocol; }

  public:
    /// Construct a TLSConnection, using the given TLSProtocol.
    Connection(std::shared_ptr<TLSProtocol> protocol);
    virtual ~Connection() = default;

    rxcpp::observable<ConnectionStatus> connectionStatus();

    rxcpp::observable<FakeVoid> shutdown();

    virtual std::string describe() const = 0;
  };

  TLSProtocol(std::shared_ptr<Parameters> parameters) : mIOservice(parameters->ensureValid().getIOservice()), mContext(parameters->getContext()) {
  }

public: // so that TLSMessageClient::onConnectFailed can access getIOservice
  std::shared_ptr<boost::asio::io_service> getIOservice() const;
protected:
  std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> createSocket() const;

private:
  std::weak_ptr<boost::asio::io_service> mIOservice;
  std::shared_ptr<boost::asio::ssl::context> mContext;
};

}
