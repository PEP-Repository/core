#include <pep/utils/LifeCycler.hpp>
#include <pep/utils/EnumUtils.hpp>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace pep {

namespace {

const std::map<LifeCycler::Status, std::set<LifeCycler::Status>> LIFECYCLE_STATUS_CHANGES {
  { LifeCycler::Status::Uninitialized, { LifeCycler::Status::Initializing, LifeCycler::Status::Finalized }},
  { LifeCycler::Status::Reinitializing, { LifeCycler::Status::Initializing, LifeCycler::Status::Finalizing }},
  { LifeCycler::Status::Initializing, { LifeCycler::Status::Reinitializing, LifeCycler::Status::Initialized, LifeCycler::Status::Finalizing }},
  { LifeCycler::Status::Initialized, { LifeCycler::Status::Reinitializing, LifeCycler::Status::Finalizing }},
  { LifeCycler::Status::Finalizing, { LifeCycler::Status::Finalized }},
  { LifeCycler::Status::Finalized, {}}
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
  assert(status_ == Status::Uninitialized || status_ >= Status::Finalizing); // Derived class should have started finalization already

  // In case derived class forgot to set the status, ensure that subscribers receive the (possibly expected/required) notification that we're finalizing
  if (status_ != Status::Uninitialized && status_ < Status::Finalizing && GetAllowedLifeCycleTransitions(status_).contains(Status::Finalizing)) {
    this->setStatus(Status::Finalizing);
  }

  // Ensure that the instance has sent the "finalized" notification before being (fully) destroyed
  if (status_ != Status::Finalized) {
    assert(GetAllowedLifeCycleTransitions(status_).contains(Status::Finalized));
    this->setStatus(Status::Finalized);
  }
}

LifeCycler::Status LifeCycler::setStatus(Status status) {
  auto result = status_;

  if (status != result) {
    if (result == Status::Initialized && status == Status::Initializing) { // When initialized instances call this->setStatus(Status::initializing) ...
      this->setStatus(Status::Reinitializing); // ... we ensure that listeners also receive a "reinitializing" notification (which they may expect)
      assert(GetAllowedLifeCycleTransitions(status_).contains(status));
    }
    else if (!GetAllowedLifeCycleTransitions(result).contains(status)) {
      throw std::runtime_error("Can't transition from life cycle status " + std::to_string(ToUnderlying(result)) + " to " + std::to_string(ToUnderlying(status)));
    }

    auto previous = status_;
    status_ = status;
    onStatusChange.notify({ previous, status_ });
  }

  return result;
}

}
