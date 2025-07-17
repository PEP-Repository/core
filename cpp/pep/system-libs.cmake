# This is an includable CMake file (as opposed to a standalone CMakeLists.txt)
# so that it doesn't introduce a separate variable scope. Thus, when external
# libraries set variables (e.g. containing include dirs and library paths) into
# their PARENT_SCOPE, those variables will be available where they're needed
# and not just in this file.

# ---------- Threads ----------

find_package(Threads)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# ---------- Boost ----------

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_LOG_USE_NATIVE_SYSLOG")
endif()

# Tell Boost::Process to use std::filesystem
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_PROCESS_USE_STD_FS")
