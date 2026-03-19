#include <pep/async/IoContextThread.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>

#include <boost/asio/io_context.hpp>

namespace pep {

namespace {
void RunIoContext(std::shared_ptr<boost::asio::io_context> io_context) noexcept {
  try {
    LOG("RunIoContext", debug) << "running io_context: " << io_context.get() << std::endl;
    io_context->run();
    LOG("RunIoContext", debug) << "stopping io_context" << std::endl;
    return;
  } catch (...) {
    LOG("RunIoContext", severity_level::critical) << "Terminating application due to uncaught exception on I/O context thread: "
      << GetExceptionMessage(std::current_exception());
  }
  //NOLINTNEXTLINE(concurrency-mt-unsafe) There's currently not a better way to handle this
  std::exit(EXIT_FAILURE);
}

}

void IoContextThread::swapStateWith(IoContextThread& other) noexcept {
  std::swap(thread_, other.thread_);
  std::swap(guard_, other.guard_);
}

IoContextThread::IoContextThread(IoContextThread&& other) noexcept
  : thread_() {
  this->swapStateWith(other);
}

IoContextThread::IoContextThread(std::shared_ptr<boost::asio::io_context> io_context)
  : guard_(std::make_unique<WorkGuard>(*io_context)), thread_(&RunIoContext, io_context) {
}

IoContextThread& IoContextThread::operator =(IoContextThread other) {
  this->swapStateWith(other);
  return *this;
}

void IoContextThread::allowTermination() noexcept {
  guard_.reset();
}

void IoContextThread::detach() {
  thread_.detach();
}
void IoContextThread::join() {
  thread_.join();
}

}
