#include <pep/async/WorkerPool.hpp>
#include <pep/utils/Log.hpp>

#include <boost/asio/io_context.hpp>
#include <thread>
#include <pep/utils/ThreadUtil.hpp>

static const std::string LOG_TAG("WorkerPool");

namespace pep {

WorkerPool::WorkerPool()
  : mIoContext(std::make_unique<boost::asio::io_context>()), mWorkGuard(std::make_unique<WorkGuard>(*mIoContext)) {
  unsigned nThreads = std::thread::hardware_concurrency();
  LOG(LOG_TAG, debug) << "Using " << nThreads << " worker threads";
  mThreads.reserve(nThreads);
  for (unsigned i = 0; i < nThreads; i++) {
    mThreads.emplace_back(
      [this, i]() {
        ThreadName::Set("WorkerPool" + std::to_string(i));
        mIoContext->run();
      });
  }
}

WorkerPool::~WorkerPool() {
  mWorkGuard.reset();
  mIoContext->stop();
  for (auto& thread : mThreads)
    thread.join();
  mThreads.clear();
}

rxcpp::observe_on_one_worker WorkerPool::worker() {
  return observe_on_asio(*mIoContext);
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
