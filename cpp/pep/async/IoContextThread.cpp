#include <pep/async/IoContextThread.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/ThreadUtil.hpp>

#include <boost/asio/io_context.hpp>

namespace pep {

namespace {

const std::string LOG_TAG = "I/O context thread";

void RunIoContext(std::shared_ptr<boost::asio::io_context> io_context, std::stop_token stop) noexcept {
  try {
    ThreadName::Set("I/O context");
    LOG(LOG_TAG, debug) << "starting io_context " << io_context << " on thread " << std::this_thread::get_id();
    std::stop_callback onStop(std::move(stop), [io_context] {
      LOG(LOG_TAG, debug) << "stopping io_context...";
      io_context->stop();
      });
    io_context->run();
    LOG(LOG_TAG, debug) << "io_context stopped";
    return;
  } catch (...) {
    LOG(LOG_TAG, severity_level::critical) << "Terminating application due to uncaught exception on I/O context thread: "
      << GetExceptionMessage(std::current_exception());
  }
  //NOLINTNEXTLINE(concurrency-mt-unsafe) There's currently not a better way to handle this
  std::exit(EXIT_FAILURE);
}

}

IoContextThread::IoContextThread(std::shared_ptr<boost::asio::io_context> io_context)
  : guard_(std::make_unique<WorkGuard>(*io_context)) {
  // Make work guard (in initializer list above) before starting the thread (below) to prevent the I/O context from running out of work immediately
  thread_ = std::jthread([io_context](std::stop_token stop) { RunIoContext(io_context, std::move(stop)); });
}

void IoContextThread::stop(bool force) noexcept {
  guard_.reset(); // Allow io_context::run to terminate
  if (force) {
    thread_ = std::jthread(); // Make our thread_ instance clean up and join
  }
}

}
