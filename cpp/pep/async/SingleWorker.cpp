#include <pep/async/SingleWorker.hpp>
#include <boost/asio/io_context.hpp>

namespace pep {

void SingleWorker::ensureRunning() {
  if(!mWorkGuard) {
    mWorkGuard = std::make_unique<WorkGuard>(*mIoContext);
  }
  if(mIoContext->stopped()) {
    if (mThread.joinable()) {
      mThread.join();
    }
    mIoContext->restart();
  }
  if(!mThread.joinable()) {
    mThread = std::thread(
            [this]() { //Since `this` owns mThread, `this` is guaranteed to exist while the thread is running (as long as we don't std::move or detach the thread)
              mIoContext->run();
            });
  }
}

SingleWorker::SingleWorker()
  : mIoContext(std::make_unique<boost::asio::io_context>()) {
}

SingleWorker::~SingleWorker() {
  std::lock_guard lockGuard(mMutex); // Concurrently calling join() on the same thread object from multiple threads constitutes a data race that results in undefined behavior. (https://en.cppreference.com/w/cpp/thread/thread/join)
  mWorkGuard.reset();
  if(mThread.joinable()) {
    mThread.join();
  }
}

}
