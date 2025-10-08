#include <pep/async/RxRequireNonEmpty.hpp>
#include <cassert>

namespace pep {

namespace {

void AssertNonEmpty(size_t size) {
  assert(size != 0U);
}

void ValidateNonEmpty(size_t size) {
  if (size == 0U) {
    throw std::runtime_error("Observable was required to emit items but did not");
  }
}

}

RxRequireNonEmpty::RxRequireNonEmpty(bool assertOnly)
  : RxProvideCount(assertOnly ? AssertNonEmpty : ValidateNonEmpty) {
}

}
