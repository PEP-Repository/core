#include <pep/utils/Event.hpp>
#include <cassert>

namespace pep {

EventSubscription::EventSubscription(EventSubscription&& other) noexcept
  : mContract(std::move(other.mContract)) {
}

EventSubscription& EventSubscription::operator=(EventSubscription&& other) noexcept {
  this->cancel();
  mContract = std::move(other.mContract);
  return *this;
}

EventSubscription::~EventSubscription() noexcept {
  this->cancel();
}

bool EventSubscription::active() noexcept {
  if (mContract == nullptr) {
    return false;
  }
  return mContract->active();
}

void EventSubscription::cancel() noexcept {
  if (mContract != nullptr) {
    mContract->cancel();
    mContract.reset();
  }
}

}
