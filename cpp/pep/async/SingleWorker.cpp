#include <pep/async/SingleWorker.hpp>

namespace pep {

void SingleWorker::ensureRunning() {
  if(!mWorkGuard) {
    mWorkGuard.emplace(boost::asio::make_work_guard(mIoService));
  }
  if(mIoService.stopped()) {
    if (mThread.joinable()) {
      mThread.join();
    }
    mIoService.restart();
  }
  if(!mThread.joinable()) {
    mThread = std::thread(
            [this]() { //Since `this` owns mThread, `this` is guaranteed to exist while the thread is running (as long as we don't std::move or detach the thread)
              mIoService.run();
            });
  }
}

SingleWorker::~SingleWorker() {
  std::lock_guard lockGuard(mMutex); // Concurrently calling join() on the same thread object from multiple threads constitutes a data race that results in undefined behavior. (https://en.cppreference.com/w/cpp/thread/thread/join)
  mWorkGuard = std::nullopt;
  if(mThread.joinable()) {
    mThread.join();
  }
}

}