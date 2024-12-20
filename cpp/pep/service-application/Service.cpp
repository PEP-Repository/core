#include <pep/service-application/Service.hpp>

namespace pep {

[[maybe_unused]]
static const std::string LOG_TAG = "Service";

ServiceBase::ServiceBase(Configuration config)
  : mConfig(std::move(config)) {
}

void ServiceBase::run() {
#ifdef NO_TLS
  LOG(LOG_TAG, critical) << "NOT USING TLS!";
  throw std::runtime_error("TLS must be enabled");
#else
  mIoService = std::make_shared<boost::asio::io_service>();
  auto listener = createListener(mIoService, mConfig);
  mIoService->run();
#endif
}

void ServiceBase::stop() {
  mIoService->stop();
}
}
