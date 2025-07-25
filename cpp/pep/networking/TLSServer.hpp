#pragma once

#include <cstdint>
#include <filesystem>

#include <boost/asio/ip/tcp.hpp>
#include <boost/type_traits/is_base_of.hpp>

#include <pep/networking/TLSMessageProtocol.hpp>

namespace pep {

class TLSServerParameters {
private:
  uint16_t listenPort_ = 0;
  X509IdentityFilesConfiguration tlsIdentity_;

protected:
  explicit TLSServerParameters(const Configuration& config); // Configured TLS certificate must be issued by PEP intermediate CA
  explicit TLSServerParameters(const X509IdentityFilesConfiguration& tlsIdentity) : tlsIdentity_(tlsIdentity) {} // Certificate may be issued by any CA

public:
  void setListenPort(uint16_t value) { listenPort_ = value; }
  uint16_t getListenPort() const { return listenPort_; }

  const std::filesystem::path& getTlsCertificateFile() const noexcept { return tlsIdentity_.getCertificateChainFilePath(); }
  const std::filesystem::path& getTlsPrivateKeyFile() const noexcept { return tlsIdentity_.getPrivateKeyFilePath(); }

  void check() const {
    if (listenPort_ == 0) {
      throw std::runtime_error("Listen port is not set");
    }
  }
};

template <class TProtocol>
class TLSServer : public TProtocol, public std::enable_shared_from_this<TLSServer<TProtocol>> {
  static_assert(boost::is_base_of<TLSProtocol, TProtocol>::value, "Inherit this class only with TProtocol types that inherit TLSProtocol");
  static_assert(boost::is_base_of<TLSProtocol::Connection, typename TProtocol::Connection>::value, "Inherit this class only with TProtocol types whose Connection inherits TLSProtocol::Connection");
  static_assert(boost::is_base_of<TLSProtocol::Parameters, typename TProtocol::Parameters>::value, "Inherit this class only with TProtocol types whose Parameters inherit TLSProtocol::Parameters");

public:
  class Parameters : public TLSServerParameters, public TProtocol::Parameters {
  protected:
    Parameters(const X509IdentityFilesConfiguration& tlsIdentity) : TLSServerParameters(tlsIdentity) {}

    Parameters(std::shared_ptr<boost::asio::io_context> io_context, const Configuration& config)
      : TLSServerParameters(config) {
      this->setIoContext(io_context);
    }

  public:
    void check() const override {
      TLSServerParameters::check();
      TProtocol::Parameters::check();
    }
  };

  class Connection : public TProtocol::Connection {
  public:
    Connection(std::shared_ptr<TLSServer> server) : TProtocol::Connection(server) {}
  protected:
    std::shared_ptr<TLSServer> getServer() const noexcept { return std::static_pointer_cast<TLSServer>(TProtocol::Connection::getProtocol()); }
    std::string describe() const override {
      auto descr = this->getServer()->describe() + " Listener connected to ";
      try {
        descr += TProtocol::Connection::getSocket().remote_endpoint().address().to_string();
      } catch (const boost::system::system_error& ex) { // remote_endpoint may throw, e.g. when not connected
        using namespace std::literals;
        // Simplify message (probably just disconnected)
        auto code = ex.code();
        boost::system::error_code withoutLocation(code.value(), code.category());
        descr += "[error: "s + withoutLocation.what() + "]";
      }
      return descr;
    }
  };

  virtual ~TLSServer() = default;

protected:
  TLSServer(std::shared_ptr<Parameters> parameters) : TProtocol(parameters), mListenPort(parameters->getListenPort()) {}

  uint16_t getListenPort() const noexcept { return mListenPort; }
  virtual std::string describe() const = 0;

private:
  uint16_t mListenPort;
};

class TLSListenerBase {
private:
  boost::asio::ip::tcp::acceptor acceptor;

protected:
  void listen();
  void accept(std::shared_ptr<TLSProtocol::Connection> new_connection, const boost::system::error_code& error);

  virtual std::shared_ptr<TLSProtocol::Connection> createConnection() = 0;

  TLSListenerBase(std::shared_ptr<TLSServerParameters> serverParameters, std::shared_ptr<TLSProtocol::Parameters> parameters);

public:
  virtual ~TLSListenerBase() = default;
};

template<class TServer>
class TLSListener : public TLSListenerBase {
  static_assert(boost::is_base_of<TLSProtocol, TServer>::value, "TServer must inherit TLSProtocol");
  static_assert(boost::is_base_of<TLSProtocol::Connection, typename TServer::Connection>::value, "Inherit this class only with TServer types whose Connection inherits pep::TLSProtocol::Connection");
  static_assert(boost::is_base_of<TLSProtocol::Parameters, typename TServer::Parameters>::value, "Inherit this class only with TServer types whose Parameters inherit pep::TLSProtocol::Parameters");
  static_assert(boost::is_base_of<TLSServerParameters, typename TServer::Parameters>::value, "Inherit this class only with TServer types whose Parameters inherit pep::TLSServerParameters");

  std::shared_ptr<TServer> mServer;

 protected:
  std::shared_ptr<TLSProtocol::Connection> createConnection() override {
    return std::make_shared<typename TServer::Connection>(mServer);
  }

 private:
  TLSListener(std::shared_ptr<typename TServer::Parameters> parameters)
    : TLSListenerBase(parameters, parameters), mServer(std::make_shared<TServer>(parameters)) {
  }

 public:
  static std::shared_ptr<TLSListener<TServer>> Create(std::shared_ptr<typename TServer::Parameters> parameters) {
    auto retval = std::shared_ptr<TLSListener<TServer>>(new TLSListener<TServer>(parameters));
    retval->listen();
    return retval;
  }
};

}
