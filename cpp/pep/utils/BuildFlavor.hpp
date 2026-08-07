#pragma once

// Identifiers for the flavors we differentiate.
#define PEP_DEBUG_BUILD_FLAVOR    1
#define PEP_RELEASE_BUILD_FLAVOR  2

// Define our PEP_BUILD_FLAVOR macro according to the canonical but mind-bending NDEBUG macro. #if (!defined(NDEBUG)) anyone?
#ifdef NDEBUG
# define PEP_BUILD_FLAVOR PEP_RELEASE_BUILD_FLAVOR
#else
# define PEP_BUILD_FLAVOR PEP_DEBUG_BUILD_FLAVOR
#endif

// Convenience macros.
#define PEP_BUILD_HAS_DEBUG_FLAVOR()    (PEP_BUILD_FLAVOR == PEP_DEBUG_BUILD_FLAVOR)
#define PEP_BUILD_HAS_RELEASE_FLAVOR()  (PEP_BUILD_FLAVOR == PEP_RELEASE_BUILD_FLAVOR)

// Deal with Microsoft's (!) nonstandard _DEBUG macro: see https://stackoverflow.com/a/2290616
#ifdef _DEBUG
# if !PEP_BUILD_HAS_DEBUG_FLAVOR()
#   error The _DEBUG macro is defined while not building for debug flavor
# endif
#endif
