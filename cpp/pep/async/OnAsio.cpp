#include <pep/utils/Log.hpp>
#include <pep/async/OnAsio.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/bind/bind.hpp>

static const std::string LOG_TAG("asio_scheduler");

namespace pep {

struct asio_scheduler : public rxcpp::schedulers::scheduler_interface {
  boost::asio::io_context& io_context;
 private:
  struct asio_scheduler_worker : public rxcpp::schedulers::worker_interface {
   private:
    boost::asio::io_context& io_context;
   public:
    asio_scheduler_worker(boost::asio::io_context& io_context) : io_context(io_context) {}
    asio_scheduler_worker(const asio_scheduler_worker&) = delete;

    clock_type::time_point now() const override {
      return clock_type::now();
    }
    void schedule(const rxcpp::schedulers::schedulable& scbl) const override {
      LOG(LOG_TAG, verbose) << "schedule on io_context " << &io_context;
      post(io_context.get_executor(), [scbl] {
        LOG(LOG_TAG, verbose) << "running on io_context";
        if (scbl.is_subscribed()) {
          // allow recursion
          rxcpp::schedulers::recursion r(true);
          scbl(r.get_recurse());
        }
      });
    }
    // partly see http://stackoverflow.com/questions/11878091/delayed-action-using-boostdeadline-timer
    class sleep : public std::enable_shared_from_this<sleep> {
      boost::asio::steady_timer timer;
      const rxcpp::schedulers::schedulable scbl;
      void action(const boost::system::error_code& e) {
        LOG(LOG_TAG, debug) << "timeout on io_context";
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
      sleep(boost::asio::io_context& io_context, const rxcpp::schedulers::schedulable& scbl) : timer(io_context), scbl(scbl) {
      }
      void start(clock_type::time_point when) {
        timer.expires_at(when);
        timer.async_wait(boost::bind(&sleep::action, shared_from_this(), boost::asio::placeholders::error));
      }
    };

    void schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable& scbl) const override {
      LOG(LOG_TAG, verbose) << "after on io_context" << &io_context;
      auto s = std::make_shared<sleep>(io_context, scbl);
      s->start(when);
    }
  };
  std::shared_ptr<asio_scheduler_worker> wi;
 public:
  asio_scheduler(boost::asio::io_context& io_context)
    : io_context(io_context), wi(std::make_shared<asio_scheduler_worker>(io_context)) {}
  asio_scheduler(const asio_scheduler&) = delete;

  clock_type::time_point now() const override {
    return clock_type::now();
  }
  rxcpp::schedulers::worker create_worker(rxcpp::composite_subscription cs) const override {
    return { std::move(cs), wi };
  }
};

rxcpp::observe_on_one_worker observe_on_asio(boost::asio::io_context& io_context) {
  rxcpp::schedulers::scheduler instance = rxcpp::schedulers::make_scheduler<asio_scheduler>(io_context);
  return rxcpp::observe_on_one_worker{ instance };
}

}
