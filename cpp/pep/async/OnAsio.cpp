#include <pep/utils/Log.hpp>
#include <pep/async/OnAsio.hpp>

#include <boost/bind/bind.hpp>

namespace pep {

struct asio_scheduler : public rxcpp::schedulers::scheduler_interface {
  boost::asio::io_service& service;
 private:
  typedef asio_scheduler this_type;
  asio_scheduler(const this_type&);
  struct asio_scheduler_worker : public rxcpp::schedulers::worker_interface {
   private:
    boost::asio::io_service& service;
    typedef asio_scheduler_worker this_type;
    asio_scheduler_worker(const this_type&);
   public:
    asio_scheduler_worker(boost::asio::io_service& service) : service(service) {
    }
    clock_type::time_point now() const override {
      return clock_type::now();
    }
    void schedule(const rxcpp::schedulers::schedulable& scbl) const override {
      LOG("asio_scheduler", debug) << "schedule on io_service " << &service;
      service.post([scbl] {
        LOG("asio_scheduler", debug) << "running on io_service";
        if (scbl.is_subscribed()) {
          // allow recursion
          rxcpp::schedulers::recursion r(true);
          scbl(r.get_recurse());
        }
      });
    }
    // partly see http://stackoverflow.com/questions/11878091/delayed-action-using-boostdeadline-timer
    class sleep : public std::enable_shared_from_this<sleep> {
      boost::asio::deadline_timer timer;
      const rxcpp::schedulers::schedulable scbl;
      void action(const boost::system::error_code& e) {
        LOG("asio_scheduler", debug) << "timeout on io_service";
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
      sleep(boost::asio::io_service& service, const rxcpp::schedulers::schedulable& scbl) : timer(service), scbl(scbl) {
      }
      void start(clock_type::time_point when) {
        timer.expires_from_now(boost::posix_time::microseconds((std::chrono::time_point_cast<std::chrono::microseconds>(when) - std::chrono::time_point_cast<std::chrono::microseconds>(clock_type::now())).count()));
        timer.async_wait(boost::bind(&sleep::action, shared_from_this(), boost::asio::placeholders::error));
      }
    };

    void schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable& scbl) const override {
      LOG("asio_scheduler", debug) << "after on io_service" << &service;
      auto s = std::make_shared<sleep>(service, scbl);
      s->start(when);
    }
  };
  std::shared_ptr<asio_scheduler_worker> wi;
 public:
  asio_scheduler(boost::asio::io_service& service)
    : service(service), wi(std::make_shared<asio_scheduler_worker>(service)) {
  }
  clock_type::time_point now() const override {
    return clock_type::now();
  }
  rxcpp::schedulers::worker create_worker(rxcpp::composite_subscription cs) const override {
    return { std::move(cs), wi };
  }
};

rxcpp::observe_on_one_worker observe_on_asio(boost::asio::io_service& service) {
  rxcpp::schedulers::scheduler instance = rxcpp::schedulers::make_scheduler<asio_scheduler>(service);
  return rxcpp::observe_on_one_worker{ instance };
}

}
