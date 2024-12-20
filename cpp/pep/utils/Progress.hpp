#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stack>
#include <stdint.h>
#include <string>

namespace pep {

class Progress : public std::enable_shared_from_this<Progress> {
public:
  typedef std::function<void(const Progress&)> OnChange;
  typedef std::function<void(std::shared_ptr<const Progress>)> OnCreation;

private:
  mutable std::optional<OnChange> mOnChange; // TODO: allow multiple listeners
  uintmax_t mTotalSteps;
  std::optional<uintmax_t> mCurrentStep;
  std::optional<std::string> mCurrentStepName;
  std::weak_ptr<const Progress> mCurrentStepChild;

  inline explicit Progress(uintmax_t totalSteps) noexcept : mTotalSteps(totalSteps) {}
  inline uintmax_t getCompletedSteps() const { return mCurrentStep.value_or(0U); }

protected:
  void onChange() const;
  void onChildChange(const Progress& child);

public:
  inline bool done() const { return this->getCompletedSteps() >= mTotalSteps; }

  void advance(uintmax_t steps = 1U, const std::optional<std::string>& newStepName = std::nullopt);
  inline void advance(const std::string& newStepName) { this->advance(1U, newStepName); }
  void advanceToCompletion();

  OnCreation push();

  std::string describe() const;
  std::stack<std::shared_ptr<const Progress>> getState() const;

  void onChange(const OnChange& notify) const;

  static std::shared_ptr<Progress> Create(uintmax_t totalSteps, const OnCreation& onCreation = [](std::shared_ptr<const Progress>) {});
};

}
