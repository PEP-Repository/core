#include <pep/async/RxRequireCount.hpp>

namespace pep {

void RxRequireCount::ValidateMax(size_t count, size_t max) {
  if (count > max) {
    throw std::runtime_error("Observable emitted " + std::to_string(count) + " item(s) but expected at most " + std::to_string(max));
  }
}

void RxRequireCount::ValidateMin(size_t count, size_t min) {
  if (count < min) {
    throw std::runtime_error("Observable emitted " + std::to_string(count) + " item(s) but expected at least " + std::to_string(min));
  }

}

}
