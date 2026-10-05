#pragma once

#include <pep/utils/Event.hpp>
#include <algorithm>
#include <iterator>
#include <stack>
#include <stdint.h>
#include <string>

namespace pep {

class Progress : public std::enable_shared_from_this<Progress> {
public:
  using OnCreation = std::function<void(std::shared_ptr<const Progress>)>;

private:
  uintmax_t totalSteps_;
  std::optional<uintmax_t> currentStep_;
  std::optional<std::string> currentStepName_;
  std::weak_ptr<const Progress> currentStepChild_;
  EventSubscription currentStepChildOnChangeSubscription_;

  inline explicit Progress(uintmax_t totalSteps) noexcept : totalSteps_(totalSteps) {}
  inline uintmax_t getCompletedSteps() const { return currentStep_.value_or(0U); }

protected:
  void onChildChange(const Progress& child);

public:
  inline bool done() const { return this->getCompletedSteps() >= totalSteps_; }

  void advance(uintmax_t steps = 1U, const std::optional<std::string>& newStepName = std::nullopt);
  inline void advance(const std::string& newStepName) { this->advance(1U, newStepName); }
  void advanceToCompletion();

  OnCreation push();

  std::string describe() const;
  std::stack<std::shared_ptr<const Progress>> getState() const;

  const Event<Progress, const Progress&> onChange;

  static std::shared_ptr<Progress> Create(uintmax_t totalSteps, const OnCreation& onCreation = [](std::shared_ptr<const Progress>) {});
};

}
