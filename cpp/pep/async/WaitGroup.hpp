#pragma once

#include <mutex>
#include <vector>
#include <string>
#include <memory>
#include <exception>
#include <functional>
#include <unordered_map>

#include <boost/core/noncopyable.hpp>

#include <rxcpp/rx-lite.hpp>

#include <pep/async/CreateObservable.hpp>
#include <pep/utils/Shared.hpp>

namespace pep {

class ActionAlreadyFinishedException : public std::exception {
 public:
  ActionAlreadyFinishedException(const std::string& what) : mWhat(what) {}
  const char* what() const noexcept override {
    return mWhat.c_str();
  }
 private:
  std::string mWhat;
};

// A WaitGroup waits for a collection of actions to finish.
class WaitGroup : public std::enable_shared_from_this<WaitGroup>, public SharedConstructor<WaitGroup>, private boost::noncopyable {
  friend class SharedConstructor<WaitGroup>;

 public:
  class Action {
   friend class WaitGroup;

   public:
    void done() const;

   private:
    size_t mId;
    std::string mDescription;
    std::shared_ptr<WaitGroup> mWg;

    Action(std::shared_ptr<WaitGroup> wg,
      size_t id, const std::string& description) :
      mId(id), mDescription(description), mWg(wg) { }
  };

  // Add a new action to wait for.  Call Action.done() to signal the action
  // is finished.
  Action add(const std::string& description = "");

  // Wait for all actions to complete.
  void wait(std::function<void(void)> callback);

  // Adapt the given observable, such that it only emits when the
  // actions for the waitgroup are done.
  template<typename T>
  rxcpp::observable<T> delayObservable(
          std::function<rxcpp::observable<T>(void)> cb) {
    auto that = shared_from_this();
    return CreateObservable<T>(
      [that, cb = std::move(cb)](rxcpp::subscriber<T> subscriber) {
      that->wait([cb = std::move(cb), subscriber = std::move(subscriber)] {
        cb().subscribe(std::move(subscriber));
      });
    });
  }

 protected:
  friend void WaitGroup::Action::done() const;
  void finish(size_t id);

 private:
  WaitGroup() = default;
  std::vector<std::function<void(void)>> mWaiters;
  std::unordered_map<size_t,std::string> mUnfinishedActions;
  std::mutex mLock;
  size_t mNextActionId = 0;
  bool mWaited = false;
};

}
