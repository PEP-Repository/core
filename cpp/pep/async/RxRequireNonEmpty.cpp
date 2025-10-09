#include <pep/async/RxRequireNonEmpty.hpp>
#include <cassert>

namespace pep {

namespace {

void ValidateNonEmpty(size_t size) {
  if (size == 0U) {
    throw std::runtime_error("Observable was required to emit items but did not");
  }
}

}

RxRequireNonEmpty::RxRequireNonEmpty()
  : RxProvideCount(&ValidateNonEmpty) {
}

}
