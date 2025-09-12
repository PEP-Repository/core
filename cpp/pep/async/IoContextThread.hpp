#pragma once

#include <functional>
#include <memory>
#include <thread>

#include <pep/async/IoContext_fwd.hpp>

namespace pep {

class IoContextThread {
 private:
  std::thread thread_;

  IoContextThread(std::shared_ptr<boost::asio::io_context> io_context, std::function<bool()> keepRunning);

 public:
  IoContextThread();
  IoContextThread(IoContextThread&& other) noexcept;
  IoContextThread(const IoContextThread&) = delete;

  explicit IoContextThread(std::shared_ptr<boost::asio::io_context> io_context);
  IoContextThread(std::shared_ptr<boost::asio::io_context> io_context, bool* keepRunning);

  IoContextThread& operator =(IoContextThread other);

  void join();
  void detach();
};

}
