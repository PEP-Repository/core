#include <pep/gui/QTrxGui.hpp>

#include <QApplication>
#include <QTimer>

void postToMainThread(std::function<void()>&& fun);

void postDelayedToMainThread(std::chrono::milliseconds when, std::function<void()>&& fun);

rxcpp::schedulers::scheduler make_gui_scheduler();

// from http://stackoverflow.com/questions/21646467/how-to-execute-a-functor-or-a-lambda-in-a-given-thread-in-qt-gcd-style
void postToMainThread(std::function<void()>&& fun) {
  QObject signalSource;
  QObject::connect(&signalSource, &QObject::destroyed, qApp, [fun = std::move(fun)](QObject*) {
    fun();
  });
}

void postDelayedToMainThread(std::chrono::milliseconds when, std::function<void()>&& fun) {
  QTimer* timer = new QTimer(qApp);
  QObject::connect(timer, &QTimer::timeout, qApp, [timer, fun = std::move(fun)]() {
    delete timer;
    fun();
  });
  timer->start(static_cast<int>(when.count()));
}

struct gui_scheduler : public rxcpp::schedulers::scheduler_interface {
 private:
  struct gui_scheduler_worker : public rxcpp::schedulers::worker_interface {
    gui_scheduler_worker() = default;
    gui_scheduler_worker(const gui_scheduler_worker&) = delete;

    clock_type::time_point now() const override {
      return clock_type::now();
    }

    void schedule(const rxcpp::schedulers::schedulable& scbl) const override {
      postToMainThread([scbl] {
        if (scbl.is_subscribed()) {
          // allow recursion
          rxcpp::schedulers::recursion r(true);
          scbl(r.get_recurse());
        }
      });
      //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) False positive
    }

    void schedule(clock_type::time_point when, const rxcpp::schedulers::schedulable& scbl) const override {
      postDelayedToMainThread(std::chrono::duration_cast<std::chrono::milliseconds>(when - clock_type::now()), [scbl] {
        if (scbl.is_subscribed()) {
          // allow recursion
          rxcpp::schedulers::recursion r(true);
          scbl(r.get_recurse());
        }
      });
      //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) False positive
    }
  };

  std::shared_ptr<gui_scheduler_worker> wi;

 public:
  gui_scheduler()
    : wi(std::make_shared<gui_scheduler_worker>()) {
  }
  gui_scheduler(const gui_scheduler&) = delete;

  clock_type::time_point now() const override {
    return clock_type::now();
  }

  rxcpp::schedulers::worker create_worker(rxcpp::composite_subscription cs) const override {
    return {std::move(cs), wi};
  }
};

rxcpp::schedulers::scheduler make_gui_scheduler() {
  rxcpp::schedulers::scheduler instance = rxcpp::schedulers::make_scheduler<gui_scheduler>();
  return instance;
}

rxcpp::observe_on_one_worker observe_on_gui() {
  return rxcpp::observe_on_one_worker{make_gui_scheduler()};
}
