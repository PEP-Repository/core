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

class AsioScheduler : public rxcpp::schedulers::scheduler_interface {
private:
  class Worker;

  boost::asio::io_context& ioContext_;
  std::shared_ptr<Worker> wi_;

 public:
  AsioScheduler(boost::asio::io_context& io_context)
    : ioContext_(io_context), wi_(std::make_shared<Worker>(io_context)) {}
  AsioScheduler(const AsioScheduler&) = delete;

  clock_type::time_point now() const override {
    return clock_type::now();
  }

  rxcpp::schedulers::worker create_worker(rxcpp::composite_subscription cs) const override;
};

class AsioScheduler::Worker : public rxcpp::schedulers::worker_interface {
private:
  class Sleep;
  boost::asio::io_context& ioContext_;

public:
  Worker(boost::asio::io_context& io_context) : ioContext_(io_context) {}
  Worker(const Worker&) = delete;

  clock_type::time_point now() const override {
    return clock_type::now();
  }

  void schedule(const rxcpp::schedulers::schedulable& scbl) const override {
    PEP_LOG(LogTag, Severity::Verbose) << "schedule on io_context " << &ioContext_;
    post(ioContext_, [scbl] {
      PEP_LOG(LogTag, Severity::Verbose) << "running on io_context";
      if (scbl.is_subscribed()) {
        // allow recursion
        rxcpp::schedulers::recursion r(true);
        scbl(r.get_recurse());
      }
      });
  }

  void schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable& scbl) const override;
};

// partly see http://stackoverflow.com/questions/11878091/delayed-action-using-boostdeadline-timer
class AsioScheduler::Worker::Sleep : public std::enable_shared_from_this<Sleep> {
private:
  boost::asio::steady_timer timer_;
  const rxcpp::schedulers::schedulable scbl_;

  void action(const boost::system::error_code& e) {
    PEP_LOG(LogTag, Severity::Debug) << "timeout on io_context";
    if (e == boost::asio::error::operation_aborted) {
      return;
    }
    if (scbl_.is_subscribed()) {
      // allow recursion
      rxcpp::schedulers::recursion r(true);
      scbl_(r.get_recurse());
    }
  }

public:
  Sleep(boost::asio::io_context& io_context, const rxcpp::schedulers::schedulable& scbl) : timer_(io_context), scbl_(scbl) {}
  void start(clock_type::time_point when) {
    timer_.expires_at(when);
    timer_.async_wait(boost::bind(&Sleep::action, shared_from_this(), boost::asio::placeholders::error));
  }
};

rxcpp::schedulers::worker AsioScheduler::create_worker(rxcpp::composite_subscription cs) const {
  return { std::move(cs), wi_ };
}

void AsioScheduler::Worker::schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable& scbl) const {
  PEP_LOG(LogTag, Severity::Verbose) << "after on io_context" << &ioContext_;
  auto s = std::make_shared<Sleep>(ioContext_, scbl);
  s->start(when);
}

}

rxcpp::observe_on_one_worker ObserveOnAsio(boost::asio::io_context& io_context) {
  rxcpp::schedulers::scheduler instance = rxcpp::schedulers::make_scheduler<AsioScheduler>(io_context);
  return rxcpp::observe_on_one_worker{ instance };
}

}
