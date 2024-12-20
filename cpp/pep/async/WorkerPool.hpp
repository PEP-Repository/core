#pragma once

#include <boost/core/noncopyable.hpp>
#include <boost/asio/io_service.hpp>
#include <optional>

#include <rxcpp/operators/rx-observe_on.hpp>
#include <rxcpp/operators/rx-merge.hpp>

#include <vector>
#include <memory>
#include <thread>
#include <mutex>

#include <pep/async/OnAsio.hpp>
#include <pep/async/RxIterate.hpp>

namespace pep {

class WorkerPool : private boost::noncopyable {
  boost::asio::io_service mService;
  std::optional<boost::asio::io_service::work> mWork;
  std::vector<std::thread> mThreads;

  static std::shared_ptr<WorkerPool> shared;
  static std::mutex sharedMux;

 public:
  WorkerPool();
  ~WorkerPool();

  static std::shared_ptr<WorkerPool> getShared();

  rxcpp::observe_on_one_worker worker();

  // Splits the given vector into batches; runs f in parallel
  // on each of the batches and return the concatenated results
  // on the given worker.
  template<size_t batchSize, typename S, typename Functor, typename Coordination>
  rxcpp::observable<std::vector<typename std::invoke_result<Functor, S>::type>>
  batched_map(
      std::vector<S> xs,
      Coordination accWorker,
      Functor f) {
    typedef typename std::invoke_result<Functor, S>::type T;
    typedef typename std::vector<T>::iterator T_iter;
    typedef typename std::vector<S>::iterator S_iter;

    if (xs.empty())
      return rxcpp::observable<>::just(std::vector<T>());

    auto ys = std::make_shared<std::vector<T>>(xs.size());
    auto xsPtr = std::make_shared<std::vector<S>>(std::move(xs));

    struct batch_t {
        S_iter in_begin; // start of batch in *xsPtr
        S_iter in_end;   // end of batch (+1 as usual for iterators)
        T_iter out;      // start of batch in return vector ys
    };
    auto batches = std::make_shared<std::vector<batch_t>>();

    size_t nBatches = xsPtr->size() / batchSize;
    size_t lastBatchSize = xsPtr->size() % batchSize;
    if (lastBatchSize == 0) lastBatchSize = batchSize;
    else nBatches++;

    // Prepare batches
    batches->resize(nBatches);
    for (size_t i = 0; i < nBatches; i++) {
      size_t thisBatchSize = batchSize;
      if (i == nBatches - 1) // last batch
        thisBatchSize = lastBatchSize;
      auto& batch = (*batches)[i];
      batch.in_begin = xsPtr->begin() + static_cast<ptrdiff_t>(i * batchSize);
      batch.in_end = xsPtr->begin() + static_cast<ptrdiff_t>((i *  batchSize) + thisBatchSize);
      batch.out = ys->begin() + static_cast<ptrdiff_t>(i * batchSize);
    }

    // xsPtr would go out of scope after this return, which frees it and
    // thus invalidates the iterators into it.  To keep xsPtr alive, we
    // capture it in the final callback.
    return RxIterate(batches)
    .map([this, f, accWorker](batch_t batch) {
      // Handle each batch on separate worker
      return rxcpp::observable<>::just(std::move(batch))
      .observe_on(this->worker())
      .map([f](batch_t batch) {
        auto it = batch.in_begin;
        auto out = batch.out;
        while (it != batch.in_end) {
          *out = f(std::move(*it));
          it++; out++;
        }
        return true; // rxcpp doesn't like void
      }).observe_on(accWorker);
    }).merge()
    .last()
    .map([xsPtr /* keep xsPtr alive, see above */, ys](bool) {
      return std::move(*ys);
    });
  }
};

}
