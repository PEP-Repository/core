#include <pep/utils/Log.hpp>
#include <pep/async/OnAsio.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/bind/bind.hpp>

namespace pep {

namespace {

const std::string LogTag("AsioScheduler");

struct AsioScheduler : public rxcpp::schedulers::scheduler_interface {
  boost::asio::io_context& io_context;
 private:
  struct Worker : public rxcpp::schedulers::worker_interface {
   private:
    boost::asio::io_context& io_context;
   public:
    Worker(boost::asio::io_context& io_context) : io_context(io_context) {}
    Worker(const Worker&) = delete;

    clock_type::time_point now() const override {
      return clock_type::now();
    }
    void schedule(const rxcpp::schedulers::schedulable& scbl) const override {
      PEP_LOG(LogTag, Severity::Verbose) << "schedule on io_context " << &io_context;
      post(io_context.get_executor(), [scbl] {
        PEP_LOG(LogTag, Severity::Verbose) << "running on io_context";
        if (scbl.is_subscribed()) {
          // allow recursion
          rxcpp::schedulers::recursion r(true);
          scbl(r.get_recurse());
        }
      });
    }
    // partly see http://stackoverflow.com/questions/11878091/delayed-action-using-boostdeadline-timer
    class Sleep : public std::enable_shared_from_this<Sleep> {
      boost::asio::steady_timer timer;
      const rxcpp::schedulers::schedulable scbl;
      void action(const boost::system::error_code& e) {
        PEP_LOG(LogTag, Severity::Debug) << "timeout on io_context";
        if (e == boost::asio::error::operation_aborted) {
          return;
        }
        if (scbl.is_subscribed()) {
          // allow recursion
          rxcpp::schedulers::recursion r(true);
          scbl(r.get_recurse());
        }
      }
     public:
      Sleep(boost::asio::io_context& io_context, const rxcpp::schedulers::schedulable& scbl) : timer(io_context), scbl(scbl) {
      }
      void start(clock_type::time_point when) {
        timer.expires_at(when);
        timer.async_wait(boost::bind(&Sleep::action, shared_from_this(), boost::asio::placeholders::error));
      }
    };

    void schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable& scbl) const override {
      PEP_LOG(LogTag, Severity::Verbose) << "after on io_context" << &io_context;
      auto s = std::make_shared<Sleep>(io_context, scbl);
      s->start(when);
    }
  };
  std::shared_ptr<Worker> wi;
 public:
  AsioScheduler(boost::asio::io_context& io_context)
    : io_context(io_context), wi(std::make_shared<Worker>(io_context)) {}
  AsioScheduler(const AsioScheduler&) = delete;

  clock_type::time_point now() const override {
    return clock_type::now();
  }
  rxcpp::schedulers::worker create_worker(rxcpp::composite_subscription cs) const override {
    return { std::move(cs), wi };
  }
};
}

rxcpp::observe_on_one_worker observe_on_asio(boost::asio::io_context& io_context) {
  rxcpp::schedulers::scheduler instance = rxcpp::schedulers::make_scheduler<AsioScheduler>(io_context);
  return rxcpp::observe_on_one_worker{ instance };
}

}
