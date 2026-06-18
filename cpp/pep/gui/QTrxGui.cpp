#include <pep/gui/QTrxGui.hpp>

#include <QApplication>
#include <QTimer>
namespace {

// from http://stackoverflow.com/questions/21646467/how-to-execute-a-functor-or-a-lambda-in-a-given-thread-in-qt-gcd-style
void PostToMainThread(std::function<void()>&& fun) {
  QObject signalSource;
  QObject::connect(&signalSource, &QObject::destroyed, qApp, [fun = std::move(fun)](QObject*) {
    fun();
  });
}

void PostDelayedToMainThread(std::chrono::milliseconds when, std::function<void()>&& fun) {
  QTimer* timer = new QTimer(qApp);
  QObject::connect(timer, &QTimer::timeout, qApp, [timer, fun = std::move(fun)]() {
    delete timer;
    fun();
  });
  timer->start(when);
}

class GuiScheduler : public rxcpp::schedulers::scheduler_interface {
private:
  class Worker;
  std::shared_ptr<Worker> wi_;

public:
  GuiScheduler()
    : wi_(std::make_shared<Worker>()) {
  }
  GuiScheduler(const GuiScheduler&) = delete;

  clock_type::time_point now() const override {
    return clock_type::now();
  }

  rxcpp::schedulers::worker create_worker(rxcpp::composite_subscription cs) const override;
};

class GuiScheduler::Worker : public rxcpp::schedulers::worker_interface {
public:
  Worker() = default;
  Worker(const Worker&) = delete;

  clock_type::time_point now() const override {
    return clock_type::now();
  }

  void schedule(const rxcpp::schedulers::schedulable& scbl) const override {
    PostToMainThread([scbl] {
      if (scbl.is_subscribed()) {
        // allow recursion
        rxcpp::schedulers::recursion r(true);
        scbl(r.get_recurse());
      }
      });
    //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) False positive
  }

  void schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable& scbl) const override {
    PostDelayedToMainThread(duration_cast<std::chrono::milliseconds>(when - clock_type::now()), [scbl] {
      if (scbl.is_subscribed()) {
        // allow recursion
        rxcpp::schedulers::recursion r(true);
        scbl(r.get_recurse());
      }
      });
    //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) False positive
  }
};

rxcpp::schedulers::worker GuiScheduler::create_worker(rxcpp::composite_subscription cs) const {
  return { std::move(cs), wi_ };
}

}

rxcpp::observe_on_one_worker ObserveOnGui() {
  rxcpp::schedulers::scheduler instance = rxcpp::schedulers::make_scheduler<GuiScheduler>();
  return rxcpp::observe_on_one_worker{instance};
}
