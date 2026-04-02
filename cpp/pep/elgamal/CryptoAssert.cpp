#include <pep/elgamal/CryptoAssert.hpp>

const bool pep::detail::EnableCryptoAssert =
#ifdef PEP_ENABLE_CRYPTO_ASSERT
  true
#else
  false
#endif
;
