#include <pep/async/WorkerPool.hpp>
#include <pep/utils/Log.hpp>

#include <thread>

static const std::string LOG_TAG("WorkerPool");

namespace pep {

WorkerPool::WorkerPool() {
  unsigned nThreads = std::thread::hardware_concurrency();
  LOG(LOG_TAG, debug) << "Using " << nThreads << " worker threads";
  mWork.emplace(mService);
  mThreads.reserve(nThreads);
  for (unsigned i = 0; i < nThreads; i++) {
    mThreads.emplace_back([this]() { mService.run(); });
  }
}

WorkerPool::~WorkerPool() {
  mWork = std::nullopt;
  mService.stop();
  for (auto& thread : mThreads)
    thread.join();
  mThreads.clear();
}

rxcpp::observe_on_one_worker WorkerPool::worker() {
  return observe_on_asio(mService);
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
