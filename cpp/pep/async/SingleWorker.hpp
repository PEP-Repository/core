#pragma once

#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <thread>
#include <boost/asio/post.hpp>
#include <optional>

namespace pep {

  /**
   * A worker thread that work can be added to, multiple times.
   * If the thread is not running, it will be started
   * If all work is finished, the thread will stop
   * If the thread is running, extra work will be queued
   */
class SingleWorker : private boost::noncopyable {
private:
  std::thread mThread;
  boost::asio::io_service mIoService;
  std::optional<boost::asio::executor_work_guard<boost::asio::io_service::executor_type>> mWorkGuard;
  std::mutex mMutex;

  void ensureRunning();

public:
  SingleWorker() = default;
  ~SingleWorker();

  // The destructor already makes sure that no move constructor or move assignment operator are implicitly declared,
  // but we make it explicit that they should not be added.
  // Moving the contained thread object may lead to problems, so the SingleWorker should also not be moved.
  SingleWorker(SingleWorker&&) = delete;
  SingleWorker& operator=(SingleWorker&&) = delete;

  template<typename F>
  void doWork(F func) {
    std::lock_guard lockGuard(mMutex); //If multiple threads enter this method, we can get problems with e.g. joining or starting mThread multiple times, or mWorkGuard being set to std::nullopt, before work has been posted.
    ensureRunning();
    boost::asio::post(mIoService, func);
    mWorkGuard = std::nullopt; //We have posted work to the io_service. Once that is finished, the io_service is allowed to stop.
  }
};

}


