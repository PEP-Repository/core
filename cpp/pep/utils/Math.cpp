#include <pep/utils/Math.hpp>

#include <stdexcept>

void pep::detail::CheckedCastThrow() {
  throw std::range_error("CheckedCast: number out of range");
}
