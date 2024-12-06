#include <pep/async/IOServiceThread.hpp>
#include <pep/utils/Log.hpp>

#include <thread>

using namespace std::chrono_literals;

namespace pep {

namespace {
void RunIOService(std::shared_ptr<boost::asio::io_service> service, std::function<bool()> keepRunning) noexcept {
  try {
    LOG("RunIOService", debug) << "running io_service: " << service.get() << std::endl;

    while (keepRunning()) {
      service->run();
      std::this_thread::sleep_for(100ms);
      service->reset();
    }
    LOG("RunIOService", debug) << "stopping io_service" << std::endl;
    return;
  } catch (const std::exception& error) {
    LOG("RunIOService", severity_level::critical) << "Terminating application due to uncaught exception on I/O service thread: " << error.what();
  } catch (...) {
    LOG("RunIOService", severity_level::critical) << "Terminating application due to uncaught exception on I/O service thread";
  }
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

IOServiceThread::IOServiceThread() : thread_() {}

IOServiceThread::IOServiceThread(IOServiceThread&& other) noexcept : thread_() {
  std::swap(thread_, other.thread_);
}

IOServiceThread::IOServiceThread(std::shared_ptr<boost::asio::io_service> service) : IOServiceThread(service, []() {
  return true;
}) {}
IOServiceThread::IOServiceThread(std::shared_ptr<boost::asio::io_service> service, bool* keepRunning) : IOServiceThread(service, GetKeepRunningCallback(keepRunning)) {}

IOServiceThread::IOServiceThread(std::shared_ptr<boost::asio::io_service> service, std::function<bool()> keepRunning) : thread_(std::thread(&RunIOService, service, keepRunning)) {}

IOServiceThread& IOServiceThread::operator =(IOServiceThread other) {
  std::swap(thread_, other.thread_);
  return *this;
}

void IOServiceThread::detach() {
  thread_.detach();
}
void IOServiceThread::join() {
  thread_.join();
}

}
