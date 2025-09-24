#pragma once

// Identifiers for the flavors we differentiate.
#define DEBUG_BUILD_FLAVOR    1
#define RELEASE_BUILD_FLAVOR  2

// Define our BUILD_FLAVOR macro according to the canonical but mind-bending NDEBUG macro. #if (!defined(NDEBUG)) anyone?
#ifdef NDEBUG
# define BUILD_FLAVOR RELEASE_BUILD_FLAVOR
#else
# define BUILD_FLAVOR DEBUG_BUILD_FLAVOR
#endif

// Convenience macros.
#define BUILD_HAS_DEBUG_FLAVOR()    (BUILD_FLAVOR == DEBUG_BUILD_FLAVOR)
#define BUILD_HAS_RELEASE_FLAVOR()  (BUILD_FLAVOR == RELEASE_BUILD_FLAVOR)

// Deal with Microsoft's (!) nonstandard _DEBUG macro: see https://stackoverflow.com/a/2290616
#ifdef _DEBUG
# if !BUILD_HAS_DEBUG_FLAVOR()
#   error The _DEBUG macro is defined while not building for debug flavor
# endif
#endif
