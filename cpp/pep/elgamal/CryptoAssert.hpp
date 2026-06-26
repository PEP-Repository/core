#pragma once

#include <cassert>

namespace pep::detail {
extern const bool EnableCryptoAssert;
}

#define PEP_CRYPTO_ASSERT(expr) \
  do { if (pep::detail::EnableCryptoAssert) { assert(expr); } } while (false)
