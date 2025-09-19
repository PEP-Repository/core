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
  auto result = std::to_string(this->getCompletedSteps() + 1U) + '/' + std::to_string(mTotalSteps);
  if (mCurrentStepName != std::nullopt) {
    result += ": " + *mCurrentStepName;
  }
  auto child = mCurrentStepChild.lock();
  if (child != nullptr) {
    result += " - " + child->describe();
  }
  return result;
}

std::stack<std::shared_ptr<const Progress>> Progress::getState() const {
  std::stack<std::shared_ptr<const Progress>> result;
  for (auto entry = shared_from_this(); entry != nullptr; entry = entry->mCurrentStepChild.lock()) {
    result.push(entry);
  }
  return result;
}

void Progress::onChildChange(const Progress& child) {
#if BUILD_HAS_DEBUG_FLAVOR()
  assert(AcquireShared(mCurrentStepChild).get() == &child);
#endif

  onChange.notify(*this);
  if (child.done()) {
    mCurrentStepChildOnChangeSubscription.cancel();
    mCurrentStepChild.reset();
  }
}

Progress::OnCreation Progress::push() {
  return[self = shared_from_this()](std::shared_ptr<const Progress> child) {
    if (!self->mCurrentStep.has_value()) {
      throw std::runtime_error("Can't push child progress onto unstarted step sequence");
    }
    if (*self->mCurrentStep > self->mTotalSteps) {
      throw std::runtime_error("Can't push child progress onto finished step sequence");
    }
    auto existing = self->mCurrentStepChild.lock();
    if (existing != nullptr) {
      throw std::runtime_error("Current step already has a child progress");
    }
    self->mCurrentStepChild = child;
    self->mCurrentStepChildOnChangeSubscription = child->onChange.subscribe([self](const Progress& sender) {self->onChildChange(sender); });
    if (child->mCurrentStep.has_value()) {
      self->onChange.notify(*self);
    }
  };
}

void Progress::advance(uintmax_t steps, const std::optional<std::string>& newStepName) {
  assert(steps > 0U);
  if (mCurrentStep.has_value()) {
    *mCurrentStep += steps;
  }
  else {
    mCurrentStep = steps - 1U;
  }
  assert(mCurrentStep <= mTotalSteps);
  mCurrentStepName = newStepName;
  mCurrentStepChild.reset();
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
