#include <pep/utils/BuildFlavor.hpp>
#include <pep/utils/Progress.hpp>
#include <pep/utils/Shared.hpp>

#include <cassert>
#include <stdexcept>

namespace pep {

std::string Progress::describe() const {
  if (this->done()) {
    return "done";
  }
  auto result = std::to_string(this->getCompletedSteps() + 1U) + '/' + std::to_string(totalSteps_);
  if (currentStepName_ != std::nullopt) {
    result += ": " + *currentStepName_;
  }
  auto child = currentStepChild_.lock();
  if (child != nullptr) {
    result += " - " + child->describe();
  }
  return result;
}

std::stack<std::shared_ptr<const Progress>> Progress::getState() const {
  std::stack<std::shared_ptr<const Progress>> result;
  for (auto entry = shared_from_this(); entry != nullptr; entry = entry->currentStepChild_.lock()) {
    result.push(entry);
  }
  return result;
}

void Progress::onChildChange(const Progress& child) {
#if PEP_BUILD_HAS_DEBUG_FLAVOR()
  assert(AcquireShared(currentStepChild_).get() == &child);
#endif

  onChange.notify(*this);
  if (child.done()) {
    currentStepChildOnChangeSubscription_.cancel();
    currentStepChild_.reset();
  }
}

Progress::OnCreation Progress::push() {
  return[self = shared_from_this()](std::shared_ptr<const Progress> child) {
    if (!self->currentStep_.has_value()) {
      throw std::runtime_error("Can't push child progress onto unstarted step sequence");
    }
    if (*self->currentStep_ > self->totalSteps_) {
      throw std::runtime_error("Can't push child progress onto finished step sequence");
    }
    auto existing = self->currentStepChild_.lock();
    if (existing != nullptr) {
      throw std::runtime_error("Current step already has a child progress");
    }
    self->currentStepChild_ = child;
    self->currentStepChildOnChangeSubscription_ = child->onChange.subscribe([self](const Progress& sender) {self->onChildChange(sender); });
    if (child->currentStep_.has_value()) {
      self->onChange.notify(*self);
    }
  };
}

void Progress::advance(uintmax_t steps, const std::optional<std::string>& newStepName) {
  assert(steps > 0U);
  if (currentStep_.has_value()) {
    *currentStep_ += steps;
  }
  else {
    currentStep_ = steps - 1U;
  }
  assert(currentStep_ <= totalSteps_);
  currentStepName_ = newStepName;
  currentStepChild_.reset();
  this->onChange.notify(*this);
}

void Progress::advanceToCompletion() {
  this->advance();
  assert(this->done());
}

std::shared_ptr<Progress> Progress::Create(uintmax_t totalSteps, const OnCreation& onCreation) {
  std::shared_ptr<Progress> result(new Progress(totalSteps));
  onCreation(result);
  return result;
}

}
