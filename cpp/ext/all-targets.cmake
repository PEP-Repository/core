# This is an includable CMake file (as opposed to a standalone CMakeLists.txt)
# so that it doesn't introduce a separate variable scope. Thus, when external
# targets set variables (e.g. containing include dirs and library paths) into
# their PARENT_SCOPE, those variables will be available where they're needed
# and not just in this file.

set(EXT_PROJECTS_DIR ${CMAKE_CURRENT_LIST_DIR})
INCLUDE_DIRECTORIES(SYSTEM ${EXT_PROJECTS_DIR}) # Allow header inclusion using #include <some-lib/blah.hpp>

# Use PEP compiler flags for external projects
SET(BASE_C_FLAGS "${CMAKE_C_FLAGS}")
SET(BASE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

add_subdirectory(${EXT_PROJECTS_DIR}/entities)
add_subdirectory(${EXT_PROJECTS_DIR}/panda)
add_subdirectory(${EXT_PROJECTS_DIR}/rxcpp)
