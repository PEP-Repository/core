#include <pep/async/IoContextThread.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <boost/asio/io_context.hpp>

using namespace std::chrono_literals;

namespace pep {

namespace {
void RunIoContext(std::shared_ptr<boost::asio::io_context> io_context, std::function<bool()> keepRunning) noexcept {
  try {
    LOG("RunIoContext", debug) << "running io_context: " << io_context.get() << std::endl;

    while (keepRunning()) {
      io_context->run();
      std::this_thread::sleep_for(100ms);
      io_context->restart();
    }
    LOG("RunIoContext", debug) << "stopping io_context" << std::endl;
    return;
  } catch (...) {
    LOG("RunIoContext", severity_level::critical) << "Terminating application due to uncaught exception on I/O context thread: "
      << GetExceptionMessage(std::current_exception());
  }
  //NOLINTNEXTLINE(concurrency-mt-unsafe) There's currently not a better way to handle this
  std::exit(EXIT_FAILURE);
}

std::function<bool()> GetKeepRunningCallback(bool* keepRunning) {
  if (keepRunning == nullptr) {
    throw std::runtime_error("Thread termination flag must be a non-NULL pointer");
  }
  return [keepRunning]() {
    return *keepRunning;
  };
}
}

IoContextThread::IoContextThread() : thread_() {}

IoContextThread::IoContextThread(IoContextThread&& other) noexcept : thread_() {
  std::swap(thread_, other.thread_);
}

IoContextThread::IoContextThread(std::shared_ptr<boost::asio::io_context> io_context) : IoContextThread(io_context, []() {
  return true;
}) {}
IoContextThread::IoContextThread(std::shared_ptr<boost::asio::io_context> io_context, bool* keepRunning) : IoContextThread(io_context, GetKeepRunningCallback(keepRunning)) {}

IoContextThread::IoContextThread(std::shared_ptr<boost::asio::io_context> io_context, std::function<bool()> keepRunning) : thread_(std::thread(&RunIoContext, io_context, keepRunning)) {}

IoContextThread& IoContextThread::operator =(IoContextThread other) {
  std::swap(thread_, other.thread_);
  return *this;
}

void IoContextThread::detach() {
  thread_.detach();
}
void IoContextThread::join() {
  thread_.join();
}

}
