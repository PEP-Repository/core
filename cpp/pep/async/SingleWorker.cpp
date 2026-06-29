#include <pep/async/SingleWorker.hpp>
#include <boost/asio/io_context.hpp>

namespace pep {

void SingleWorker::ensureRunning() {
  if(!workGuard_) {
    workGuard_ = std::make_unique<WorkGuard>(*ioContext_);
  }
  if(ioContext_->stopped()) {
    if (thread_.joinable()) {
      thread_.join();
    }
    ioContext_->restart();
  }
  if(!thread_.joinable()) {
    thread_ = std::thread(
            [this]() { //Since `this` owns thread_, `this` is guaranteed to exist while the thread is running (as long as we don't std::move or detach the thread)
              ioContext_->run();
            });
  }
}

SingleWorker::SingleWorker()
  : ioContext_(std::make_unique<boost::asio::io_context>()) {
}

SingleWorker::~SingleWorker() {
  std::lock_guard lockGuard(mutex_); // Concurrently calling join() on the same thread object from multiple threads constitutes a data race that results in undefined behavior. (https://en.cppreference.com/w/cpp/thread/thread/join)
  workGuard_.reset();
  if(thread_.joinable()) {
    thread_.join();
  }
}

}
