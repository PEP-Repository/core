#include <pep/utils/LifeCycler.hpp>
#include <pep/utils/TypeTraits.hpp>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace pep {

namespace {

const std::map<LifeCycler::Status, std::set<LifeCycler::Status>> LIFECYCLE_STATUS_CHANGES {
  { LifeCycler::Status::uninitialized, { LifeCycler::Status::initializing, LifeCycler::Status::finalized }},
  { LifeCycler::Status::reinitializing, { LifeCycler::Status::initializing, LifeCycler::Status::finalizing }},
  { LifeCycler::Status::initializing, { LifeCycler::Status::reinitializing, LifeCycler::Status::initialized, LifeCycler::Status::finalizing }},
  { LifeCycler::Status::initialized, { LifeCycler::Status::reinitializing, LifeCycler::Status::finalizing }},
  { LifeCycler::Status::finalizing, { LifeCycler::Status::finalized }},
  { LifeCycler::Status::finalized, {}}
};

const std::set<LifeCycler::Status>& GetAllowedLifeCycleTransitions(LifeCycler::Status from) {
  auto result = LIFECYCLE_STATUS_CHANGES.find(from);
  if (result == LIFECYCLE_STATUS_CHANGES.cend()) {
    throw std::runtime_error("Unsupported life cycle status " + std::to_string(ToUnderlying(from)));
  }
  return result->second;
}

}

LifeCycler::~LifeCycler() noexcept {
  assert(mStatus == Status::uninitialized || mStatus >= Status::finalizing); // Derived class should have started finalization already

  // In case derived class forgot to set the status, ensure that subscribers receive the (possibly expected/required) notification that we're finalizing
  if (mStatus != Status::uninitialized && mStatus < Status::finalizing && GetAllowedLifeCycleTransitions(mStatus).contains(Status::finalizing)) {
    this->setStatus(Status::finalizing);
  }

  // Ensure that the instance has sent the "finalized" notification before being (fully) destroyed
  if (mStatus != Status::finalized) {
    assert(GetAllowedLifeCycleTransitions(mStatus).contains(Status::finalized));
    this->setStatus(Status::finalized);
  }
}

LifeCycler::Status LifeCycler::setStatus(Status status) {
  auto result = mStatus;

  if (status != result) {
    if (result == Status::initialized && status == Status::initializing) { // When initialized instances call this->setStatus(Status::initializing) ...
      this->setStatus(Status::reinitializing); // ... we ensure that listeners also receive a "reinitializing" notification (which they may expect)
      assert(GetAllowedLifeCycleTransitions(mStatus).contains(status));
    }
    else if (!GetAllowedLifeCycleTransitions(result).contains(status)) {
      throw std::runtime_error("Can't transition from life cycle status " + std::to_string(ToUnderlying(result)) + " to " + std::to_string(ToUnderlying(status)));
    }

    auto previous = mStatus;
    mStatus = status;
    onStatusChange.notify({ previous, mStatus });
  }

  return result;
}

}
