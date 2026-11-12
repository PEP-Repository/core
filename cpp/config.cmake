# This file defines settings applicable to all (C and) C++ targets (including external ones).
cmake_minimum_required(VERSION 3.20...4.3)

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

  # Parallel compilation.
  # By default, MSBuild builds multiple projects in parallel (-maxCpuCount, in Visual Studio:
  #  Tools > Options > Projects and Solutions > Build and Run > Maximum number of parallel project builds),
  #  and the compiler compiles multiple sources of each project in parallel (/MP), so the number of compiler processes multiplies.
  #  See https://learn.microsoft.com/en-us/cpp/build/reference/mp-build-with-multiple-processes#the-msbuild-tool
  # MSBuild's Multi-ToolTask (MTT) scheduler instead starts one cl.exe per source file itself, and with
  #  EnforceProcessCountAcrossBuilds it limits the number of compiler processes across all projects and MSBuild instances
  #  on the machine. See https://devblogs.microsoft.com/cppblog/improved-parallelism-in-msbuild/
  # The limit is MSBuild property MultiProcMaxCount, which defaults to environment variable MultiProcMaxCount, or else to
  #  CL_MPCount, which Visual Studio sets from Tools > Options > Projects and Solutions > VC++ Project Settings >
  #  Maximum concurrent C++ compilations (0 for the number of logical CPUs).
  # For their definitions, see Microsoft.Cpp.Common.props and Microsoft.CppCommon.targets in the Visual Studio
  #  installation, under MSBuild\Microsoft\VC\v180.
  # Note that dependencies built by Conan are not affected by this setting: use the environment variables
  #  UseMultiToolTask=true and EnforceProcessCountAcrossBuilds=true for that, as ci_cd/windows-ci-build.bat does.
  # CMAKE_VS_GLOBALS sets properties in the Globals property group of each generated project, so this also applies to
  #  builds from the IDE. These definitions override environment variables of the same name,
  #  see https://learn.microsoft.com/en-us/visualstudio/msbuild/how-to-use-environment-variables-in-a-build
  #  They also take precedence over the defaults in Microsoft.Cpp.Common.props, which only apply to empty properties.
  if(CMAKE_GENERATOR MATCHES "^Visual Studio")
    option(USE_MSBUILD_MULTI_TOOL_TASK "Use MSBuild's Multi-ToolTask scheduler to limit the number of compiler processes machine-wide (ignored with COMPILE_SEQUENTIALLY)" ON)
  endif()

  # We don't add a plain /MP here (which would mean "one process per logical CPU"), because:
  #  - Conan's CMakeToolchain already adds /MP<tools.build:jobs> for Visual Studio generators,
  #    and a plain /MP after it would override that setting.
  #  - With MTT, /MP is dropped anyway.
  option(COMPILE_SEQUENTIALLY "Compile one source at a time (per project)")
  if(COMPILE_SEQUENTIALLY)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP1")
    if(CMAKE_GENERATOR MATCHES "^Visual Studio")
      # MTT drops /MP, so explicitly disable MTT for /MP1 to take effect,
      #  also when it is enabled via an environment variable (e.g. by ci_cd/windows-ci-build.bat).
      list(APPEND CMAKE_VS_GLOBALS "UseMultiToolTask=false")
    endif()
  elseif(USE_MSBUILD_MULTI_TOOL_TASK)
    list(APPEND CMAKE_VS_GLOBALS "UseMultiToolTask=true" "EnforceProcessCountAcrossBuilds=true")
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
