# This file defines settings applicable to all (C and) C++ targets (including external ones).
cmake_minimum_required(VERSION 3.20...4.2)

enable_language(C CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
    message(FATAL_ERROR "Require at least gcc 7.0 (or different compiler)")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
    message(FATAL_ERROR "Require at least clang 6.0 (or different compiler)")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

cmake_policy(SET CMP0135 NEW) # Ignore timestamps of files in downloaded archives to make sure content changes are picked up: https://cmake.org/cmake/help/latest/policy/CMP0135.html

# Deal with issues specific to the Microsoft compiler. Compiler sniffing code was copied from CMake's FindBoost module.
if (MSVC)
  option(SHOW_COMPILE_TIME "Include compilation time in output")
  if (SHOW_COMPILE_TIME)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Bt+")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /time")
    set(CMAKE_EXE_MODULE_FLAGS "${CMAKE_EXE_MODULE_FLAGS} /time")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /time")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /time")
  endif()

  # Use correct version for __cplusplus macro
  add_compile_options("/Zc:__cplusplus")

  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
  set(CMAKE_EXE_MODULE_FLAGS "${CMAKE_EXE_MODULE_FLAGS} /ignore:4099")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /ignore:4099")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} /ignore:4099")

  # Allow multi-processor compilation: see https://blogs.msdn.microsoft.com/visualstudio/2010/03/07/tuning-c-build-parallelism-in-vs2010/
  option(COMPILE_SEQUENTIALLY "Compile one source at a time")
  if (COMPILE_SEQUENTIALLY)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP1")
  else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  endif()

  # Fix C1128: number of sections exceeded object file format limit: compile with /bigobj
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
  # Get rid of warnings about unsafe standard functions such as _open and _ftime64
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_NO_WARNINGS")
  # Get rid of warnings about passing "unchecked iterators" such as pointers to standard functions such as std::copy. See https://msdn.microsoft.com/en-us/library/aa985965.aspx and e.g. https://stackoverflow.com/a/1301343
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SCL_SECURE_NO_WARNINGS")
  # Circumvent faulty C++ language version detection by Google test framework, which results in use of tr1::tuple class, which produces
  # warning C4996: 'std::tr1': warning STL4002: The non-Standard std::tr1 namespace and TR1-only machinery are deprecated and will be REMOVED.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DGTEST_LANG_CXX11=1")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DGTEST_HAS_TR1_TUPLE=0")
  # warning STL4019: The member std::fpos::seekpos() is non-Standard, and is preserved only for compatibility with workarounds for old versions of Visual C++.
  # Can be safely silenced for Boost 1.68 but the desired solution is to upgrade boost to 1.69+
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_SILENCE_FPOS_SEEKPOS_DEPRECATION_WARNING")
endif()

option(WITH_TESTS "Build tests" ON)
if(NOT WITH_TESTS)
  message(STATUS "WITH_TESTS=${WITH_TESTS}")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  # To build on Apple x86_64
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden -fvisibility=hidden")
  # set(CMAKE_OSX_ARCHITECTURES "x86_64")
  # set(CMAKE_OSX_DEPLOYMENT_TARGET 12.0)
endif()

option(HTTPSERVER_WITH_TLS "Build HTTPServer with support for TLS" On)
if(HTTPSERVER_WITH_TLS)
    add_definitions(-DHTTPSERVER_WITH_TLS)
endif()
