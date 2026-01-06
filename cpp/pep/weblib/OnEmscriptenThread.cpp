#include <pep/weblib/OnEmscriptenThread.hpp>

#include <pep/utils/Log.hpp>

#include <emscripten/proxying.h>
#include <emscripten/threading.h>

static const std::string LOG_TAG("emscripten_scheduler");

namespace pep {

class emscripten_scheduler : public rxcpp::schedulers::scheduler_interface {
  class worker_interface : public rxcpp::schedulers::worker_interface {
    ::pthread_t thread_;
    mutable emscripten::ProxyingQueue queue_;

  public:
    explicit worker_interface(::pthread_t thread) : thread_{thread} {}

    clock_type::time_point now() const override {
      return clock_type::now();
    }

    void schedule(const rxcpp::schedulers::schedulable& scbl) const override {
      LOG(LOG_TAG, verbose) << "schedule on emscripten thread " << thread_;

      bool success = queue_.proxyAsync(thread_, [scbl] {
        LOG(LOG_TAG, verbose) << "schedule on emscripten thread";
        if (scbl.is_subscribed()) {
          // allow recursion
          rxcpp::schedulers::recursion r(true);
          scbl(r.get_recurse());
        }
      });
      if (!success) {
        throw std::runtime_error("Failed to proxy to Emscripten thread");
      }
    }

    void schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable& scbl) const override {
      throw std::logic_error("Cannot schedule delayed work to Emscripten thread");
    }
  };

  std::shared_ptr<worker_interface> workerInterface_;

public:
  explicit emscripten_scheduler(::pthread_t thread)
    : workerInterface_(std::make_shared<worker_interface>(thread)) {}

  clock_type::time_point now() const override {
    return clock_type::now();
  }

  rxcpp::schedulers::worker create_worker(rxcpp::composite_subscription cs) const override {
    return {std::move(cs), workerInterface_};
  }
};

rxcpp::observe_on_one_worker observe_on_emscripten(::pthread_t thread) {
  rxcpp::schedulers::scheduler instance = rxcpp::schedulers::make_scheduler<emscripten_scheduler>(thread);
  return rxcpp::observe_on_one_worker{instance};
}

rxcpp::observe_on_one_worker observe_on_emscripten_main_thread() {
  return observe_on_emscripten(::emscripten_main_runtime_thread_id());
}

}
