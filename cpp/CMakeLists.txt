# Settings for all C++ targets (including external ones)
include("config.cmake" NO_POLICY_SCOPE)

# Include third-party targets in our build
include("ext/all-targets.cmake")

# Include PEP targets in our build
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR} # Allow header inclusion using #include <pep/subdir/blah.hpp>
  ${CMAKE_CURRENT_BINARY_DIR} # For generated sources such as QT's ui_xyz.h
)
add_subdirectory(pep)
