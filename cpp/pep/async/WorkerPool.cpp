#include <pep/async/WorkerPool.hpp>
#include <pep/utils/Log.hpp>

#include <boost/asio/io_context.hpp>
#include <thread>
#include <pep/utils/ThreadUtil.hpp>

namespace {
const std::string LogTag("WorkerPool");

constexpr unsigned MaxThreads =
#ifdef __EMSCRIPTEN__
  // Emscripten threads (workers) seem more expensive
  8 // Keep consistent with -sPTHREAD_POOL_SIZE
#else
  16
#endif
;
}

namespace pep {

WorkerPool::WorkerPool()
  : ioContext_(std::make_unique<boost::asio::io_context>()), workGuard_(std::make_unique<WorkGuard>(*ioContext_)) {
  unsigned nThreads = std::min(std::thread::hardware_concurrency(), MaxThreads);
  PEP_LOG(LogTag, Severity::Debug) << "Using " << nThreads << " worker threads";
  threads_.reserve(nThreads);
  for (unsigned i = 0; i < nThreads; i++) {
    threads_.emplace_back(
      [this, i]() {
        ThreadName::Set("WorkerPool" + std::to_string(i));
        ioContext_->run();
      });
  }
}

WorkerPool::~WorkerPool() {
  workGuard_.reset();
  ioContext_->stop();
  for (auto& thread : threads_)
    thread.join();
  threads_.clear();
}

rxcpp::observe_on_one_worker WorkerPool::worker() {
  return observe_on_asio(*ioContext_);
}

std::shared_ptr<WorkerPool> WorkerPool::getShared() {
  std::lock_guard<std::mutex> g(sharedMux);
  if (shared == nullptr)
    shared = std::make_shared<WorkerPool>();
  return shared;
}

std::shared_ptr<WorkerPool> WorkerPool::shared = nullptr;
std::mutex WorkerPool::sharedMux;

}
