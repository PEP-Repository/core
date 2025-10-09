#include <pep/async/CreateObservable.hpp>
#include <pep/async/RxRequireCount.hpp>

namespace pep {

void RxRequireCount::ValidateMax(size_t count, size_t max) {
  if (count > max) {
    throw std::runtime_error("Observable emitted " + std::to_string(count) + " item(s) but expected at most " + std::to_string(max));
  }
}

rxcpp::observable<FakeVoid> RxRequireCount::ValidateMin(std::shared_ptr<size_t> count, size_t min) {
  return CreateObservable<FakeVoid>([count, min](rxcpp::subscriber<FakeVoid> subscriber) {
    if (*count < min) {
      subscriber.on_error(std::make_exception_ptr(std::runtime_error("Observable emitted " + std::to_string(*count) + " item(s) but expected at least " + std::to_string(min))));
    }
    else {
      subscriber.on_completed();
    }
    });
}

}
