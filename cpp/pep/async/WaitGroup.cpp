#include <pep/async/WaitGroup.hpp>

#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/map.hpp>

#include <iostream>


namespace pep {

namespace {

const std::string LogTag = "WaitGroup";

}

WaitGroup::Action WaitGroup::add(const std::string& description) {
  std::lock_guard<std::mutex> lock(lock_);
  if (waited_) {
    throw std::runtime_error("Cannot add actions to WaitGroup after callbacks have been registered");
  }

  auto id = nextActionId_++;
  unfinishedActions_[id] = description;
  return WaitGroup::Action(shared_from_this(), id, description);
}

void WaitGroup::wait(std::function<void(void)> callback) {
  {
    std::lock_guard<std::mutex> lock(lock_);
    waited_ = true;
    if (!unfinishedActions_.empty()) {
      PEP_LOG(LogTag, Severity::Verbose) << this << " waiter is waiting for unfinished actions: " << boost::algorithm::join(unfinishedActions_ | boost::adaptors::map_values, ", ");
      waiters_.push_back(callback);
      return;
    }
  }

  callback();
}

void WaitGroup::Action::done() const {
  wg_->finish(id_);
}

void WaitGroup::finish(size_t id) {
  std::vector<std::function<void(void)>> cbs;

  {
    std::lock_guard<std::mutex> lock(lock_);

    auto position = unfinishedActions_.find(id);
    if (position == unfinishedActions_.end())
      throw ActionAlreadyFinishedException("Action was already finished");

    if (!waiters_.empty()) {
      PEP_LOG(LogTag, Severity::Verbose) << this << " finished action: " << position->second;
    }
    unfinishedActions_.erase(position);

    if (!unfinishedActions_.empty())
      return;

    std::swap(cbs, waiters_);
  }

  if (!cbs.empty()) {
    PEP_LOG(LogTag, Severity::Verbose) << this << " invoking " << cbs.size() << " waiter(s)";
    for (const auto& cb : cbs)
      cb();
  }
}


}
