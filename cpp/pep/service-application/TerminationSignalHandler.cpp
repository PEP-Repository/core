#include <pep/service-application/TerminationSignalHandler.hpp>
#include <pep/utils/Log.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>

#include <csignal>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace pep {

namespace {

const std::string LogTag = "Service application";

}

TerminationSignalHandler::TerminationSignalHandler(std::function<void()> onTermination)
  : ioContext_(std::make_shared<boost::asio::io_context>()), signals_(*ioContext_, SIGINT, SIGTERM) {
  signals_.async_wait([this, onTermination = std::move(onTermination)](const boost::system::error_code& error, int signal) {
    if (error) { // Cancelled: we're being destroyed
      return;
    }
    signals_.clear(); // Restore default handling, so that a second signal kills the process
    PEP_LOG(LogTag, Severity::Info) << "Received " << (signal == SIGINT ? "SIGINT" : "SIGTERM") << ": shutting down";
    onTermination();
    });

  // Started last, so that the handler can't run before signals_ is set up
  thread_.emplace("Termination signals", ioContext_);
}

}
