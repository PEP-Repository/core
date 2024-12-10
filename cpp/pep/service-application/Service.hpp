#pragma once

#include <pep/networking/TLSServer.hpp>

#include <boost/asio/io_service.hpp>

namespace pep {

class ServiceBase : boost::noncopyable {
private:
  std::filesystem::path configurationFile_;
  std::shared_ptr<boost::asio::io_service> mIoService;

protected:
  explicit ServiceBase(const std::filesystem::path& configurationFile);
  virtual std::shared_ptr<TLSListenerBase> createListener(std::shared_ptr<boost::asio::io_service> io_service, const Configuration &config) const = 0;

public:
  virtual ~ServiceBase() = default;
  void run();
  void stop();
};

template <typename TServer>
class Service : public ServiceBase {
protected:
  std::shared_ptr<TLSListenerBase> createListener(std::shared_ptr<boost::asio::io_service> io_service, const Configuration &config) const override {
    auto parameters = std::make_shared<typename TServer::Parameters>(io_service, config);
    return TLSListener<TServer>::Create(parameters);
  }

public:
  explicit Service(const std::filesystem::path& configurationFile) : ServiceBase(configurationFile) {}
};

}
