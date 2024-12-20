#pragma once

#include <functional>
#include <memory>
#include <thread>

#include <boost/asio/io_service.hpp>

namespace pep {

class IOServiceThread {
 private:
  std::thread thread_;

  IOServiceThread(std::shared_ptr<boost::asio::io_service> service, std::function<bool()> keepRunning);
  IOServiceThread(const IOServiceThread&) = delete;

 public:
  IOServiceThread();
  IOServiceThread(IOServiceThread&& other) noexcept;

  explicit IOServiceThread(std::shared_ptr<boost::asio::io_service> service);
  IOServiceThread(std::shared_ptr<boost::asio::io_service> service, bool* keepRunning);

  IOServiceThread& operator =(IOServiceThread other);

  void join();
  void detach();
};

}
