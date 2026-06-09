#include <pep/weblib/OnEmscriptenThread.hpp>

#include <pep/utils/Log.hpp>
#include <pep/weblib/SetTimeout.hpp>
#include <pep/weblib/ThreadPrintable.hpp>

#include <emscripten/proxying.h>
#include <emscripten/threading.h>

using namespace pep;

static const std::string LOG_TAG("emscripten_scheduler");

namespace {

class emscripten_scheduler : public rxcpp::schedulers::scheduler_interface {
  class worker_interface : public rxcpp::schedulers::worker_interface {
    ::pthread_t thread_;
    std::shared_ptr<emscripten::ProxyingQueue> queue_ = std::make_shared<emscripten::ProxyingQueue>();

  public:
    explicit worker_interface(::pthread_t thread) : thread_{thread} {}

    clock_type::time_point now() const override {
      return clock_type::now();
    }

    void schedule(const rxcpp::schedulers::schedulable& scbl) const override {
      LOG(LOG_TAG, verbose) << "schedule on thread " << weblib::ThreadPrintable{thread_}
        << " from thread " << weblib::ThreadPrintable{}
        << (thread_ == ::pthread_self() ? " (queuing for current thread)" : "");
      const bool success = queue_->proxyAsync(thread_, [scbl] {
        LOG(LOG_TAG, verbose) << "running on thread " << weblib::ThreadPrintable{};
        if (!scbl.is_subscribed()) { return; }

        // allow recursion
        rxcpp::schedulers::recursion r(true);
        scbl(r.get_recurse());
      });
      if (!success) {
        throw std::runtime_error("Failed to proxy to an Emscripten thread");
      }
    }

    void schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable& scbl) const override {
      using namespace std::chrono;

      LOG(LOG_TAG, verbose) << "schedule after " << ceil<milliseconds>(when - clock_type::now())
        << " on thread " << weblib::ThreadPrintable{thread_}
        << " from thread " << weblib::ThreadPrintable{}
        << (thread_ == ::pthread_self() ? " (queuing for current thread)" : "");
      const bool success = queue_->proxyAsync(thread_, [queue = queue_, when, scbl] {
        if (!scbl.is_subscribed()) { return; }

        // Compute delay after proxying.
        // If non-positive, setTimeout will schedule for the next JS event cycle.
        // I would do `duration_cast<duration<double, std::milli>>`,
        // but then the callback sometimes executes around 0.5 ms too early for some reason.
        const auto delay = ceil<milliseconds>(when - clock_type::now());

        auto onUnsubscribe = std::make_shared<rxcpp::subscription::weak_state_type>();

        auto timeout = weblib::SetTimeout(delay, [scbl, onUnsubscribe] {
          scbl.remove(*onUnsubscribe);
          LOG(LOG_TAG, verbose) << "running after delay on thread " << weblib::ThreadPrintable{};
          if (!scbl.is_subscribed()) { return; }

          // allow recursion
          rxcpp::schedulers::recursion r(true);
          scbl(r.get_recurse());
        });
        LOG(LOG_TAG, verbose) << "running on thread " << weblib::ThreadPrintable{}
          << ", delaying " << delay;

        // Cancel timer when unsubscribed, also to prevent code from executing after runtime shutdown
        *onUnsubscribe = scbl.add([queue, thread = ::pthread_self(), timeout] {
          const bool success = queue->proxyAsync(thread, [timeout] {
            timeout.cancel();
            LOG(LOG_TAG, verbose) << "canceled timer on thread " << weblib::ThreadPrintable{};
          });
          if (!success) {
            LOG(LOG_TAG, debug) << "Failed to proxy timer cancelation on thread " << weblib::ThreadPrintable{thread};
          }
        });
      });
      if (!success) {
        throw std::runtime_error("Failed to proxy to an Emscripten thread");
      }
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

}

rxcpp::observe_on_one_worker pep::weblib::observe_on_emscripten(::pthread_t thread) {
  rxcpp::schedulers::scheduler instance = rxcpp::schedulers::make_scheduler<emscripten_scheduler>(thread);
  return rxcpp::observe_on_one_worker{instance};
}

rxcpp::observe_on_one_worker pep::weblib::observe_on_emscripten_main_thread() {
  return observe_on_emscripten(::emscripten_main_runtime_thread_id());
}
