// This is the single translation unit that is compiled for panda.
// A single translation unit yields a ~15% speed-up for scalar multiplication
// (presumably) due to inlined field operations.

#include "fe25519.c"
#include "group.c"
#include "crypto_hash_sha512.c"
#include "scalar.c"
