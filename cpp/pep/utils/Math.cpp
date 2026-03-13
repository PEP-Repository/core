#include <pep/utils/Math.hpp>

#include <stdexcept>

namespace pep {

namespace detail {
void CheckedCastThrow() {
  throw std::range_error("CheckedCast: number out of range");
}
}

}
