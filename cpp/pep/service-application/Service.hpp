#pragma once

#include <pep/networking/TLSServer.hpp>
#include <pep/async/IoContext_fwd.hpp>
#include <pep/utils/Configuration.hpp>

namespace pep {

class ServiceBase : boost::noncopyable {
private:
  Configuration mConfig;
  std::shared_ptr<boost::asio::io_context> mIoContext;

protected:
  explicit ServiceBase(Configuration config);
  virtual std::shared_ptr<TLSListenerBase> createListener(std::shared_ptr<boost::asio::io_context> io_context, const Configuration &config) const = 0;

public:
  virtual ~ServiceBase() = default;
  void run();
  void stop();
};

template <typename TServer>
class Service : public ServiceBase {
protected:
  std::shared_ptr<TLSListenerBase> createListener(std::shared_ptr<boost::asio::io_context> io_context, const Configuration &config) const override {
    auto parameters = std::make_shared<typename TServer::Parameters>(io_context, config);
    return TLSListener<TServer>::Create(parameters);
  }

public:
  explicit Service(Configuration config) : ServiceBase(std::move(config)) {}
};

}
