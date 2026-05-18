#include <pep/async/WaitGroup.hpp>

#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/map.hpp>

#include <iostream>

static const std::string LOG_TAG = "WaitGroup";

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
      LOG(LOG_TAG, verbose) << this << " waiter is waiting for unfinished actions: " << boost::algorithm::join(mUnfinishedActions | boost::adaptors::map_values, ", ");
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

    if (!mWaiters.empty()) {
      LOG(LOG_TAG, verbose) << this << " finished action: " << position->second;
    }
    mUnfinishedActions.erase(position);

    if (!mUnfinishedActions.empty())
      return;

    std::swap(cbs, mWaiters);
  }

  if (!cbs.empty()) {
    LOG(LOG_TAG, verbose) << this << " invoking " << cbs.size() << " waiter(s)";
    for (const auto& cb : cbs)
      cb();
  }
}


}
