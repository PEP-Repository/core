#pragma once

#include <mutex>
#include <vector>
#include <string>
#include <memory>
#include <exception>
#include <functional>
#include <unordered_map>

#include <boost/core/noncopyable.hpp>

#include <pep/utils/Shared.hpp>

namespace pep {

class ActionAlreadyFinishedException : public std::exception {
 public:
  ActionAlreadyFinishedException(const std::string& what) : what_(what) {}
  const char* what() const noexcept override {
    return what_.c_str();
  }
 private:
  std::string what_;
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
    size_t id_;
    std::string description_;
    std::shared_ptr<WaitGroup> wg_;

    Action(std::shared_ptr<WaitGroup> wg,
      size_t id, const std::string& description) :
      id_(id), description_(description), wg_(wg) { }
  };

  // Add a new action to wait for.  Call Action.done() to signal the action
  // is finished.
  Action add(const std::string& description = "");

  // Wait for all actions to complete.
  void wait(std::function<void(void)> callback);

 protected:
  friend void WaitGroup::Action::done() const;
  void finish(size_t id);

 private:
  WaitGroup() = default;
  std::vector<std::function<void(void)>> waiters_;
  std::unordered_map<size_t,std::string> unfinishedActions_;
  std::mutex lock_;
  size_t nextActionId_ = 0;
  bool waited_ = false;
};

}
