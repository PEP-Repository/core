# Provide system libraries (perhaps including targets that build external code)...
include("system-libs.cmake")

option(WARNINGS_AS_ERROR "Treat compiler & linter warnings as errors" ON)
option(DEPRECATED_WARNINGS_AS_ERROR "With WARNINGS_AS_ERROR, also issue errors for deprecated symbols" ON)

option(ENABLE_CLANG_TIDY "Enable clang-tidy")
if(ENABLE_CLANG_TIDY)
  message(STATUS "Clang-Tidy enabled")
  set(clang_tidy_profile_dir "${CMAKE_BINARY_DIR}/clang-tidy-profile/")
  option(PROFILE_CLANG_TIDY "Individually profile (benchmark) clang-tidy checks and store results to ${clang_tidy_profile_dir}. Build target clang-tidy-stats for stats")
  set(clang_tidy_opts)
  if(PROFILE_CLANG_TIDY)
    list(APPEND clang_tidy_opts "--enable-check-profile" "--store-check-profile=${clang_tidy_profile_dir}")
    add_custom_target(clang-tidy-stats
        COMMENT "Print Clang-Tidy checks performance statistics"
        COMMAND ${INVOKE_SH} "${CMAKE_CURRENT_SOURCE_DIR}/clang-tidy-stats.sh" ${clang_tidy_profile_dir}
        USES_TERMINAL
        VERBATIM
    )
    set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES ${clang_tidy_profile_dir})
  endif()
  if(WARNINGS_AS_ERROR)
    list(APPEND clang_tidy_opts "--warnings-as-errors=*")
  endif()
  if(CMAKE_COLOR_DIAGNOSTICS)
    list(APPEND clang_tidy_opts --use-color)
  endif()
  find_program(CLANG_TIDY_PROGRAM clang-tidy)
  # Recompile when .clang-tidy changes
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.clang-tidy")
  file(SHA256 "${CMAKE_SOURCE_DIR}/.clang-tidy" clang_tidy_hash)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_PROGRAM} ${clang_tidy_opts} "--extra-arg=-DPEP_DUMMY_CLANG_TIDY_HASH=${clang_tidy_hash}")
endif()

# Always enable certain compiler warnings if using either GCC or Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  function(warn_no_error language)
    foreach(warn_name IN LISTS ARGN)
      set(flag "-W${warn_name}")
      set(supported_var "FLAG_SUPPORTED_${language}_${flag}")
      escape_cache_var_name(supported_var)
      if(NOT DEFINED ${supported_var})
        include(CheckCompilerFlag)
        check_compiler_flag("${language}" "${flag}" ${supported_var})
      endif()
      if(${supported_var})
        add_compile_options("$<$<COMPILE_LANGUAGE:${language}>:-Wno-error=${warn_name}>")
      endif()
    endforeach()
  endfunction()

  # Ref: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html and https://clang.llvm.org/docs/DiagnosticsReference.html
  # Make sure to always add these
  add_compile_options(
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
  )
  # These should be reported as errors, if supported by the compiler
  add_supported_compile_options(CXX
      -Wabstract-vbase-init
      -Walloca
      -Wanon-enum-enum-conversion
      -Warray-bounds-pointer-arithmetic
      -Wassign-enum
      -Wbad-function-cast
      -Wbidi-chars=any
      -Wcast-align
      -Wcast-align=strict
      -Wcast-function-type
      -Wcast-qual
      -Wclass-varargs
      -Wcomma
      -Wcompound-token-split
      -Wconditional-uninitialized
      -Wconsumed
      -Wcovered-switch-default
      -Wdecls-in-multiple-modules
      -Wdeprecated
      -Wdeprecated-implementations
      -Wdocumentation
      -Wdocumentation-pedantic
      -Wdtor-name
      -Wduplicate-decl-specifier
      -Wduplicate-enum
      -Wduplicate-method-arg
      -Wduplicate-method-match
      -Wduplicated-cond
      -Wextra-semi-stmt
      -Wflex-array-member-not-at-end
      -Wformat-non-iso
      -Wformat-nonliteral
      -Wformat-pedantic
      -Wformat-signedness
      -Wformat-type-confusion
      -Wfour-char-constants
      -Wfunction-effects
      -Wgcc-compat
      -Wheader-hygiene
      -Wimplicit-fallthrough
      -Wincompatible-function-pointer-types-strict
      -Wincomplete-module
      -Winconsistent-missing-destructor-override
      -Winvalid-or-nonexistent-directory
      -Winvalid-utf8
      -Wleading-whitespace=spaces
      -Wlogical-op
      -Wloop-analysis
      -Wmethod-signatures
      -Wmissing-include-dirs
      -Wmissing-prototypes
      -Wmissing-variable-declarations
      -Wnon-virtual-dtor
      -Wnonportable-system-include-path
      -Wnullable-to-nonnull-conversion
      -Wold-style-cast
      -Wover-aligned
      -Wpacked
      -Wpoison-system-directories
      -Wpragmas
      -Wredundant-decls
      -Wreserved-identifier
      -Wshadow-field-in-constructor-modified
      -Wshift-sign-overflow
      -Wsign-conversion
      -Wsigned-enum-bitfield
      -Wsuggest-destructor-override
      -Wsuggest-override
      -Wsuper-class-method-mismatch
      -Wtautological-value-range-compare
      -Wthread-safety
      -Wthread-safety-negative
      -Wtrampolines
      -Wunaligned-access
      -Wundef
      -Wundef-prefix
      -Wundefined-reinterpret-cast
      -Wunreachable-code
      -Wunsupported-dll-base-class-template
      -Wunused-exception-parameter
      -Wunused-macros
      -Wvector-conversion
      -Wvla
      -Wwrite-strings
      -Wzero-as-null-pointer-constant

      -Wno-unused-parameter
  )

  if(NOT DEPRECATED_WARNINGS_AS_ERROR)
    warn_no_error(CXX
        deprecated-declarations
        deprecated-implementations
    )
  endif()
endif()

# For MSVC, bump up the warning level from level 1 (default) to 2
# TODO: See if we can bump this to level 3 or 4?
if (MSVC)
  # Ref: https://learn.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level?view=msvc-170
  add_compile_options("/W2")
endif()

# Treat warnings as errors
if (WARNINGS_AS_ERROR)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif()

    if (MSVC)
        add_compile_options("/WX")
        if(NOT DEPRECATED_WARNINGS_AS_ERROR)
          # We're disabling this warning completely since I don't think it's possible to exclude specific warnings from /WX
          add_compile_options("/wd4996")  # Your code uses a function, class member, variable, or typedef that's marked deprecated.
        endif()
    else()
      if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        # AppleClang warns about libraries passed twice to the linker, for example:
        # pepcorelib uses OpenSSL, Qt uses OpenSSL, pepLogon uses both of these, so will get OpenSSL twice.
        # This is not a problem, but AppleClang complains, so we silence that with -no_warn_duplicate_libraries.
        # To deduplicate them, there is https://cmake.org/cmake/help/latest/policy/CMP0156.html,
        # but for some reason enabling that doesn't work for our project, at least with Conan,
        # even though it does work for a minimal test project.
        set(FATAL_WARNING_OPTION "-Wl,-fatal_warnings,-no_warn_duplicate_libraries")
      else()
        set(FATAL_WARNING_OPTION "-Wl,--fatal-warnings")
      endif()

      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FATAL_WARNING_OPTION}")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${FATAL_WARNING_OPTION}")
    endif()
endif()

option(ENABLE_SANITIZERS "Enable Address Sanitizer and UB Sanitizer" OFF)
if (ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  message(STATUS "ENABLE_SANITIZERS=${ENABLE_SANITIZERS}, compiling with ASan and UBSan enabled")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
endif()

# TODO: Why do we disable this optimization when using GCC?
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-reorder-blocks -fno-reorder-blocks-and-partition")
endif()

if(NOT DEFINED PEP_VERSION_MAJOR)
  message(FATAL_ERROR "Major version is not set.")
endif()
if(NOT DEFINED PEP_VERSION_MINOR)
  message(FATAL_ERROR "Minor version is not set.")
endif()
if(NOT DEFINED PEP_VERSION_BUILD)
  message(FATAL_ERROR "Build number is not set.")
endif()
if(NOT DEFINED PEP_VERSION_REVISION)
  message(FATAL_ERROR "Revision number is not set.")
endif()

add_compile_definitions(
  "BUILD_PROJECT_PATH=\"$ENV{CI_PROJECT_PATH}\""
  "BUILD_REF=\"$ENV{CI_COMMIT_REF_NAME}\""
  "BUILD_COMMIT=\"$ENV{CI_COMMIT_SHA}\""
  "PEP_VERSION_MAJOR=${PEP_VERSION_MAJOR}"
  "PEP_VERSION_MINOR=${PEP_VERSION_MINOR}"
  "PEP_VERSION_BUILD=${PEP_VERSION_BUILD}"
  "PEP_VERSION_REVISION=${PEP_VERSION_REVISION}"
  "BUILD_TARGET=\"${CMAKE_SYSTEM_NAME}\""
)

function(add_windows_manifest target)
  if(WIN32)
    set(WINDOWS_MANIFEST_ASSEMBLY_NAME Pep.Pep.${target})
    configure_file(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/windows-app.base.manifest windows-app.${target}.manifest @ONLY)
    target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/windows-app.${target}.manifest)
  endif()
endfunction()

# Add executable that will be installed by `cmake --install --component ${name}`
# When installing, you need to specify the component to avoid errors about missing files when you did not build all targets
function(add_install_executable name)
  add_executable(${ARGV})
  add_windows_manifest(${name})
  include(GNUInstallDirs)
  install(TARGETS ${name} COMPONENT ${name}

    # Copy runtime dependencies.
    # This calls a tool on the produced binary to list what shared library names it requires,
    # and then finds these libraries in directories including ${CONAN_RUNTIME_LIB_DIRS}
    RUNTIME_DEPENDENCIES
      # Exclude these names
      PRE_EXCLUDE_REGEXES
        [[^api-ms-]]  # Skip Windows Universal C Runtime
        [[^ext-ms-]]  # Skip Windows APIs
        [[^libc\.so]] [[^libgcc_s\.so]] [[^libm\.so]] [[^libstdc\+\+\.so]]  # Skip Unix STL
      # Exclude these paths
      POST_EXCLUDE_REGEXES
        [[[/\\]system32[/\\].*\.dll$]]  # Skip Windows OS DLLs
        [[^/lib]] [[^/usr/lib]] [[^/usr/local/lib]]  # Skip Unix system shared libs
      DIRECTORIES ${CONAN_RUNTIME_LIB_DIRS}

    #TODO This may not be what we want
    BUNDLE DESTINATION "${CMAKE_INSTALL_BINDIR}"  # Required for MACOSX_BUNDLE
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_BINDIR}"  # Also for macOS
  )
endfunction()

# Add executable accepting our --autocomplete flag that should be installed
function(add_cli_install_executable name)
  set_property(GLOBAL APPEND PROPERTY cli_executables ${name})
  add_install_executable(${ARGV})
endfunction()

#

option(VERIFY_HEADERS_STANDALONE "Verify that headers #include all their dependencies" OFF)


# Fill a variable with (relative paths to) the .hpp files included in a target
function(get_target_headers variable target)
  # Iterate over the target's SOURCES
  get_target_property(srces ${target} SOURCES)
  foreach(src ${srces})
    # If the source(file)name ends with ".hpp"...
    get_filename_component(ext "${src}" LAST_EXT)
    if("${ext}" STREQUAL ".hpp")
      # ...store the path as needed by "#include <this/one/here.hpp>" directives
      file(REAL_PATH ${src} fullpath)
      file(RELATIVE_PATH relpath "${CMAKE_SOURCE_DIR}/cpp" "${fullpath}")
      list(APPEND result "${relpath}")
    endif()
  endforeach()

  # Write result to caller's variable
  set(${variable} ${result} PARENT_SCOPE)
endfunction()

# Create an additional (library) target that compiles each of the original target's .hpp files individually
# to verify that each of them provides (e.g. #includes) all its dependencies.
function(verify_headers_standalone target)
  # Only create the additional target if the option is enabled
  if(VERIFY_HEADERS_STANDALONE)
  # Only create the additional target if there are headers to verify
    get_target_headers(headers "${target}")
    if(headers)
      add_library("${target}_StandaloneHeaders")
      # Copy include directories from original target to ensure that (nested) #include directives can be resolved
      get_target_property(dirs ${target} INCLUDE_DIRECTORIES)
      target_include_directories("${target}_StandaloneHeaders" PUBLIC "${dirs}")
      # Copy linked libraries from original target to ensure that (nested) #include directives can be resolved
      get_target_property(libs ${target} INTERFACE_LINK_LIBRARIES)
      target_link_libraries("${target}_StandaloneHeaders" "${libs}")

      # For each header from the original "target", create a source that only #includes that header
      foreach(header ${headers})
        set(src "${CMAKE_BINARY_DIR}/cpp/${header}.cpp")
        file(WRITE "${src}" "#include <${header}>")
        target_sources("${target}_StandaloneHeaders" PRIVATE "${src}")
      endforeach()
    endif()
  endif()
endfunction()


##

# Given a service (base) name, prepares the build system for debugging of that service,
# adding target dependencies and copying files.
function(configure_service_debugging service_name)
    string(TOLOWER ${service_name} service_name_lowercase)
    add_dependencies(${PROJECT_NAME}${service_name} ${service_name_lowercase}_pki)
    configure_debug_command_line_parameter(${PROJECT_NAME}${service_name} ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${service_name}.json)
endfunction()

set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS ON)  # Check dependencies in target_link_libraries exist at config time

# Retrieve value from legacy cache variable
if(NOT DEFINED BUILD_CLIENT)
  set(BUILD_CLIENT ON)
endif()
option(WITH_ASSESSOR "Build Assessor GUI software" ${BUILD_CLIENT})
if(NOT WITH_ASSESSOR)
  message(STATUS "WITH_ASSESSOR=${WITH_ASSESSOR}")
endif()

# Retrieve value from legacy cache variable
if(NOT DEFINED BUILD_SERVERS)
  set(BUILD_SERVERS ON)
endif()
option(WITH_SERVERS "Build the ${PROJECT_NAME} servers" ${BUILD_SERVERS})
if(NOT WITH_SERVERS)
  message(STATUS "WITH_SERVERS=${WITH_SERVERS}")
endif()

option(WITH_BENCHMARK "Build benchmark" ON)
if(NOT WITH_BENCHMARK)
  message(STATUS "WITH_BENCHMARK=${WITH_BENCHMARK}")
endif()

option(WITH_LOGON "Build ${PROJECT_NAME}Logon" ON)
if(NOT WITH_LOGON)
  message(STATUS "WITH_LOGON=${WITH_LOGON}")
endif()

option(WITH_CASTOR "Enable Castor API functionality" ON)
if(NOT WITH_CASTOR)
  message(STATUS "WITH_CASTOR=${WITH_CASTOR}")
endif()

# Sets directory for CastorAPIKey.json if it is found inside the projects repository
if (NOT CASTOR_API_KEY_DIR AND "${CMAKE_CURRENT_LIST_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}/cpp/pep")
  get_filename_component(CORE_PARENT_DIR ${CMAKE_SOURCE_DIR} DIRECTORY)
  if (EXISTS "${CORE_PARENT_DIR}/ops/keys/CastorAPIKey.json")
    SET(CASTOR_API_KEY_DIR "${CORE_PARENT_DIR}/ops/keys")
    message(STATUS "Directory for CastorAPIKey.json is automatically defaulted to: ${CASTOR_API_KEY_DIR}")
  endif()
endif()
if (CASTOR_API_KEY_DIR)
  # Interpret (possibly relative) Castor API key dir spec in relation to where CMake was invoked
  get_filename_component(CASTOR_API_KEY_DIR ${CASTOR_API_KEY_DIR} ABSOLUTE BASE_DIR ${CMAKE_BINARY_DIR})
endif()

option(ENABLE_OAUTH_TEST_USERS "Show a list of test users for OAuth when opening the authorization endpoint in your browser." ON)
if(ENABLE_OAUTH_TEST_USERS)
    add_compile_definitions(ENABLE_OAUTH_TEST_USERS)
endif()

## Unconditional components

add_subdirectory(utils)

add_subdirectory(accessmanager)
add_subdirectory(application)
add_subdirectory(apps)
add_subdirectory(archiving)
add_subdirectory(async)
add_subdirectory(auth)
add_subdirectory(authserver)
add_subdirectory(cli)
add_subdirectory(client)
add_subdirectory(content)
add_subdirectory(core-client)
add_subdirectory(crypto)
add_subdirectory(database)
add_subdirectory(elgamal)
add_subdirectory(key-components)
add_subdirectory(keyserver)
add_subdirectory(messaging)
add_subdirectory(morphing)
add_subdirectory(networking)
add_subdirectory(proto)
add_subdirectory(registrationserver)
add_subdirectory(rsk)
add_subdirectory(rsk-pep)
add_subdirectory(serialization)
add_subdirectory(server)
add_subdirectory(storagefacility)
add_subdirectory(structure)
add_subdirectory(structuredoutput)
add_subdirectory(templating)
add_subdirectory(ticketing)
add_subdirectory(transcryptor)
add_subdirectory(versioning)

## Conditional components
# Please keep conditions in sync with conanfile.py

if(WITH_ASSESSOR)
  add_subdirectory(gui)
  add_subdirectory(elevate)
  add_subdirectory(assessor)
endif()

if(WITH_BENCHMARK)
  add_subdirectory(benchmark)
endif()

if(WITH_LOGON)
  add_subdirectory(logon)
endif()

if(WITH_CASTOR)
  add_subdirectory(castor)
  add_subdirectory(pullcastor)
endif()

if(WITH_SERVERS)
  add_subdirectory(servers)
  add_subdirectory(service-application)
endif()

## Conditional dependencies required by multiple components
# Please keep conditions in sync with conanfile.py

if(WITH_ASSESSOR OR WITH_LOGON OR WITH_SERVERS)
  add_subdirectory(httpserver)
endif()

if(WITH_ASSESSOR OR WITH_LOGON)
  add_subdirectory(oauth-client)
endif()

if(WITH_SERVERS OR WITH_CASTOR)
  add_subdirectory(metrics)
endif()


##### Disable warnings for generated sources of all pep targets #####

# See https://discourse.cmake.org/t/cmake-list-of-all-project-targets/1077/17
function(get_all_cmake_targets out_var current_dir)
  get_directory_property(targets DIRECTORY ${current_dir} BUILDSYSTEM_TARGETS)
  get_directory_property(subdirs DIRECTORY ${current_dir} SUBDIRECTORIES)

  foreach(subdir ${subdirs})
    get_all_cmake_targets(subdir_targets ${subdir})
    list(APPEND targets ${subdir_targets})
  endforeach()

  set(${out_var} ${targets} PARENT_SCOPE)
endfunction()

# Run at end of top-level CMakeLists
get_all_cmake_targets(all_targets ${CMAKE_CURRENT_LIST_DIR})

foreach(target IN LISTS all_targets)
  get_target_property(sources ${target} SOURCES)
  foreach(source IN LISTS sources)
    get_source_file_property(generated "${source}" TARGET_DIRECTORY ${target} GENERATED)
    if(generated)
      get_source_file_property(lang ${source} TARGET_DIRECTORY ${target} LANGUAGE)
      if(lang MATCHES "^(C|CXX|OBJC|OBJCXX)$")
        message(DEBUG "Disabling warnings for GENERATED ${lang} source ${source} (target ${target})")

        get_source_file_property(opts ${source} TARGET_DIRECTORY ${target} COMPILE_OPTIONS)
        if(NOT opts) # NOTFOUND
          set(opts)
        endif()

        # Prevent warning D9025 about overriding /W[n] on MSVC
        if(MSVC)
          list(APPEND opts "/W0") # Overwrite warning options including built-in /W1
        endif()

        # Disable compile warnings (works for both gcc/clang and MSVC)
        list(APPEND opts -w)

        # Disable GCC analyzer (if someone enabled it)
        if(CMAKE_${lang}_COMPILER_ID STREQUAL "GNU")
          list(APPEND opts -fno-analyzer)
        endif()

        set_source_files_properties(${source}
            TARGET_DIRECTORY ${target}
            PROPERTIES
              SKIP_LINTING ON
              COMPILE_OPTIONS "${opts}"
        )
      endif()
    endif()
  endforeach()
endforeach()
