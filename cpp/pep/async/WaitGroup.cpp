#include <pep/async/WaitGroup.hpp>

#include <iostream>

namespace pep {

WaitGroup::Action WaitGroup::add(const std::string& description) {
  std::lock_guard<std::mutex> lock(mLock);
  if (mWaited) {
    throw std::runtime_error("Cannot add actions to WaitGroup after callbacks have been registered");
  }

  auto id = mNextActionId++;
  mUnfinishedActions[id] = description;
  return WaitGroup::Action(shared_from_this(), id, description);
}

void WaitGroup::wait(std::function<void(void)> callback) {
  {
    std::lock_guard<std::mutex> lock(mLock);
    mWaited = true;
    if (!mUnfinishedActions.empty()) {
      mWaiters.push_back(callback);
      return;
    }
  }

  callback();
}

void WaitGroup::Action::done() const {
  mWg->finish(mId);
}

void WaitGroup::finish(size_t id) {
  std::vector<std::function<void(void)>> cbs;

  {
    std::lock_guard<std::mutex> lock(mLock);

    auto position = mUnfinishedActions.find(id);
    if (position == mUnfinishedActions.end())
      throw ActionAlreadyFinishedException("Action was already finished");

    mUnfinishedActions.erase(position);

    if (!mUnfinishedActions.empty())
      return;

    std::swap(cbs, mWaiters);
  }

  for (const auto& cb : cbs)
    cb();
}


}
