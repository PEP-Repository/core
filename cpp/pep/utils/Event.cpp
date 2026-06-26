#include <pep/utils/Event.hpp>
#include <cassert>

namespace pep {

EventSubscription::EventSubscription(EventSubscription&& other) noexcept
  : contract_(std::move(other.contract_)) {
}

EventSubscription& EventSubscription::operator=(EventSubscription&& other) noexcept {
  this->cancel();
  contract_ = std::move(other.contract_);
  return *this;
}

EventSubscription::~EventSubscription() noexcept {
  this->cancel();
}

bool EventSubscription::active() noexcept {
  if (contract_ == nullptr) {
    return false;
  }
  return contract_->active();
}

void EventSubscription::cancel() noexcept {
  if (contract_ != nullptr) {
    contract_->cancel();
    contract_.reset();
  }
}

}
