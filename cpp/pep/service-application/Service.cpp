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
  mIoContext = std::make_shared<boost::asio::io_context>();
  auto listener = createListener(mIoContext, mConfig);
  mIoContext->run();
#endif
}

void ServiceBase::stop() {
  mIoContext->stop();
}
}
