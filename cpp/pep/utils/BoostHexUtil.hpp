#pragma once

#include <cassert>
#include <cstddef>

namespace pep {

namespace detail {
  // The Boost (implementation) code shows that each byte is converted to two hex digits.
  // Also, from https://www.boost.org/doc/libs/1_55_0/libs/algorithm/doc/html/the_boost_algorithm_library/Misc/hex.html :
  // If the input to unhex does not contain an "even number" of hex digits, then an exception of type boost::algorithm::not_enough_input is thrown.
  const size_t BoostHexDigitsPerByte = 2U;
}

inline constexpr size_t BoostHexLength(size_t binaryLength) {
  return binaryLength * detail::BoostHexDigitsPerByte;
}

inline constexpr size_t BoostUnhexLength(size_t hexLength) {
  assert(hexLength % detail::BoostHexDigitsPerByte == 0U);
  return hexLength / detail::BoostHexDigitsPerByte;
}

}
