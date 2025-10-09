#include <pep/async/CreateObservable.hpp>
#include <pep/async/RxRequireCount.hpp>

namespace pep {

RxRequireCount::RxRequireCount(size_t min, size_t max, std::optional<std::string> errorText)
  : mMin(min), mMax(max), mErrorText(errorText.value_or("item(s)")) {
}

RxRequireCount::RxRequireCount(size_t exact, std::optional<std::string> errorText)
  : RxRequireCount(exact, exact, std::move(errorText)) {
}

void RxRequireCount::ValidateMax(size_t count, size_t max, const std::string& errorText) {
  if (count > max) {
    throw std::runtime_error("Observable emitted " + std::to_string(count) + " " + errorText + ", but expected at most " + std::to_string(max));
  }
}

rxcpp::observable<FakeVoid> RxRequireCount::ValidateMin(std::shared_ptr<size_t> count, size_t min, const std::string& errorText) {
  return CreateObservable<FakeVoid>([count, min, errorText](rxcpp::subscriber<FakeVoid> subscriber) {
    if (*count < min) {
      subscriber.on_error(std::make_exception_ptr(std::runtime_error("Observable emitted " + std::to_string(*count) + " " + errorText + ", but expected at least " + std::to_string(min))));
    }
    else {
      subscriber.on_completed();
    }
    });
}

}
