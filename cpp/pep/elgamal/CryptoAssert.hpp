#pragma once

#include <cassert>

namespace pep::detail {
extern const bool EnableCryptoAssert;
}

#define PEP_CryptoAssert(expr) \
  do { if (pep::detail::EnableCryptoAssert) { assert(expr); } } while (false)
