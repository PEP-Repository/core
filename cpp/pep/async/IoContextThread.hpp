#pragma once

#include <thread>

#include <pep/async/IoContext_fwd.hpp>
#include <pep/async/WorkGuard.hpp>

namespace pep {

class IoContextThread {
 private:
   std::shared_ptr<boost::asio::io_context> context_;
   std::unique_ptr<WorkGuard> guard_;
   std::thread thread_;

   void stopContext() noexcept;

 public:
  IoContextThread(IoContextThread&& other) noexcept = default;
  IoContextThread(const IoContextThread&) = delete;
  ~IoContextThread() noexcept;

  explicit IoContextThread(std::shared_ptr<boost::asio::io_context> io_context);

  IoContextThread& operator =(const IoContextThread&) = delete;

  void stop(bool force = false) noexcept;
};

}
