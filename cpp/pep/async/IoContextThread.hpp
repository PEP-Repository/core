#pragma once

#include <thread>

#include <pep/async/IoContext_fwd.hpp>
#include <pep/async/WorkGuard.hpp>

namespace pep {

/// @brief Runs an io_context on a separate thread, and ensures that the thread is joined before/during destruction.
class IoContextThread {
private:
   std::jthread thread_;
   std::unique_ptr<WorkGuard> guard_; // Declared after thread_ so it is destroyed before thread_ to prevent deadlock

public:
  IoContextThread(const IoContextThread&) = delete;
  IoContextThread& operator=(const IoContextThread&) = delete;

  IoContextThread(IoContextThread&& other) noexcept = default;
  IoContextThread& operator=(IoContextThread&&) = default;

  /// @brief Destructor.
  /// @remark Finalizes the object as if this->stop(true) were called: schedules the thread to be stopped (force stopping the io_context) and blocks until the thread is joined.
  ~IoContextThread() noexcept = default;

  /// @brief Constructor. (Immediately) runs the specified io_context on a separate thread of execution
  /// @param io_context The io_context to run.
  /// @remark The io_context is kept run()ning until this (IoContextThread) instance is destroyed or stop()ped.
  explicit IoContextThread(std::shared_ptr<boost::asio::io_context> io_context);

  /// @brief Schedules the thread to be stopped.
  /// @param force Whether the io_context is to be explicitly stop()ped. If not, it (and associated thread) will keep running until the io_context runs out of work.
  /// @remark Blocks until the thread is (stopped and) joined.
  void stop(bool force = false) noexcept;
};

}
