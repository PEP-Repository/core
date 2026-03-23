#pragma once

#include <thread>

#include <pep/async/IoContext_fwd.hpp>
#include <pep/async/WorkGuard.hpp>

namespace pep {

class IoContextThread {
 private:
   std::jthread thread_;
   std::unique_ptr<WorkGuard> guard_; // Declared after thread_ so it is destroyed before thread_ to prevent deadlock

 public:
  IoContextThread(IoContextThread&& other) noexcept = default;
  IoContextThread& operator=(IoContextThread&&) = default;
  ~IoContextThread() noexcept = default;

  IoContextThread(const IoContextThread&) = delete;
  IoContextThread& operator=(const IoContextThread&) = delete;

  explicit IoContextThread(std::shared_ptr<boost::asio::io_context> io_context);
  void stop(bool force = false) noexcept;
};

}
