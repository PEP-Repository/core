# This file defines settings applicable to all (C and) C++ targets (including external ones).
cmake_minimum_required(VERSION 3.20...4.2)

enable_language(C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

function(check_compiler_version compiler min_version)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "${compiler}" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS min_version)
    message(WARNING "PEP may fail to build with ${compiler} < ${min_version}. See supported environments in cpp/CONTRIBUTING.md.")
  endif()
endfunction()

check_compiler_version("AppleClang" 17)
check_compiler_version(Clang 21)
check_compiler_version(GNU 15)
check_compiler_version(MSVC 19.51)

include(CheckCXXSourceCompiles)

function(check_stdlib_version name macro min_version)
  set(CMAKE_REQUIRED_QUIET ON)
  check_cxx_source_compiles("
    #include <version>
    #if defined(${macro}) && (${macro}) < (${min_version})
      #error \"standard library too old\"
    #endif
    int main() {}
  " STDLIB_VERSION_OK_${name}_${min_version})
  if(NOT STDLIB_VERSION_OK_${name}_${min_version})
    message(WARNING "PEP may fail to build with ${name} < ${min_version}. See supported environments in cpp/CONTRIBUTING.md.")
  endif()
endfunction()

# _GLIBCXX_RELEASE is the libstdc++ major version (available since GCC 7).
check_stdlib_version(GLIBCXX _GLIBCXX_RELEASE 15)
# _LIBCPP_VERSION encodes libc++'s version as <major><minor 2 digits><patch 2 digits>, e.g. 200000 for 20.0.0.
check_stdlib_version(LIBCPP _LIBCPP_VERSION 200000)

if(BUILD_SHARED_LIBS)
  message(STATUS "BUILD_SHARED_LIBS is ON. This requires dependencies to be shared as well, to prevent ODR-violations.")
  if(MSVC)
    # MSVC doesn't export any symbols from a DLL unless explicitly annotated with __declspec(dllexport).
    # For functions, there is CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS, but this does not work for global/static variables.
    # See https://cmake.org/cmake/help/latest/prop_tgt/WINDOWS_EXPORT_ALL_SYMBOLS.html
    message(SEND_ERROR "BUILD_SHARED_LIBS is currently not supported with MSVC")
  endif()
else()
  # Prevent symbols from ending up in final executable.
  # Do not enable when using shared libraries, as in that case we would need to export specific symbols in code.
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
endif()

if(LINUX)
  include(CheckLinkerFlag)
  check_linker_flag(CXX LINKER:--disable-new-dtags disable_new_dtags_supported)
  if(disable_new_dtags_supported)
    # Default to ON when BUILD_SHARED_LIBS is set, as it's a development option triggering this issue
    set(disable_runpath_default ${BUILD_SHARED_LIBS})
    # This makes sure that transitive libraries lacking an rpath are still found
    option(DISABLE_RUNPATH "Use RPATH instead of RUNPATH. This makes sure the whole paths of (transitive) shared libraries are included." ${disable_runpath_default})
    if(DISABLE_RUNPATH)
      add_link_options("$<$<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>:LINKER:--disable-new-dtags>")
    endif()
  endif()
endif()

cmake_policy(SET CMP0135 NEW) # Ignore timestamps of files in downloaded archives to make sure content changes are picked up: https://cmake.org/cmake/help/latest/policy/CMP0135.html

# Deal with issues specific to the Microsoft compiler. Compiler sniffing code was copied from CMake's FindBoost module.
if(MSVC)
  option(SHOW_COMPILE_TIME "Include compilation time in output")
  if(SHOW_COMPILE_TIME)
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
  if(COMPILE_SEQUENTIALLY)
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

option(HTTPSERVER_WITH_TLS "Build HTTPServer with support for TLS" On)
if(HTTPSERVER_WITH_TLS)
  add_definitions(-DHTTPSERVER_WITH_TLS)
endif()
