# Provide system libraries (perhaps including targets that build external code)...
include("system-libs.cmake")

# Always enable certain compiler warnings if using either GCC or Clang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # Ref: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html and https://clang.llvm.org/docs/DiagnosticsReference.html#wall
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wconversion -Wold-style-cast")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
endif()

# For MSVC, bump up the warning level from level 1 (default) to 2
# TODO: See if we can bump this to level 3 or 4?
if (MSVC)
  # Ref: https://learn.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level?view=msvc-170
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W2")
endif()

# Treat warnings as errors (https://gitlab.pep.cs.ru.nl/pep/core/-/issues/725)
option(WARNINGS_AS_ERROR "Treat compiler warnings as errors" ON)
if (WARNINGS_AS_ERROR)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")

        # We can not treat old-style-cast warnings as errors because we use certain OpenSSL macros that use old style casts
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=old-style-cast")
    endif()

    if (MSVC)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
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

if(NOT DEFINED MAJOR_VERSION)
  message(FATAL_ERROR "Major version is not set.")
endif()
if(NOT DEFINED MINOR_VERSION)
  message(FATAL_ERROR "Minor version is not set.")
endif()

# If updating these, don't forget to update them for Go too.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_PROJECT_PATH=\\\"$ENV{CI_PROJECT_PATH}\\\"")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_REF=\\\"$ENV{CI_COMMIT_REF_NAME}\\\"")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_REV=\\\"$ENV{CI_COMMIT_SHA}\\\"")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_MAJOR_VERSION=${MAJOR_VERSION}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_MINOR_VERSION=${MINOR_VERSION}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_PIPELINE_ID=$ENV{CI_PIPELINE_ID}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_JOB_ID=$ENV{CI_JOB_ID}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBUILD_TARGET=\\\"${CMAKE_SYSTEM_NAME}\\\"")

function(add_windows_manifest target)
  set(WINDOWS_MANIFEST_ASSEMBLY_NAME Pep.Pep.${target})
  configure_file(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/windows-app.base.manifest windows-app.${target}.manifest @ONLY)
  target_sources(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/windows-app.${target}.manifest)
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

##

# Given a service (base) name, prepares the build system for debugging of that service,
# adding target dependencies and copying files.
function(configure_service_debugging service_name)
    string(TOLOWER ${service_name} service_name_lowercase)
    add_dependencies(${PROJECT_NAME}${service_name} ${service_name_lowercase}_pki)
    configure_debug_command_line_parameter(${PROJECT_NAME}${service_name} ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${service_name}.json)
endfunction()

##

add_subdirectory(utils) # Also adds Testinglib
add_subdirectory(archiving)
add_subdirectory(async)

add_subdirectory(proto) # Generate C++ protobuf classes
add_subdirectory(versioning)

add_subdirectory(serialization)
add_subdirectory(crypto)
add_subdirectory(elgamal)
add_subdirectory(rsk)
add_subdirectory(rsk-pep)
add_subdirectory(morphing)
add_subdirectory(structure)

add_subdirectory(networking)
add_subdirectory(httpserver)
add_subdirectory(templating)


option(WITH_CASTOR "Enable Castor API functionality" ON)
message(STATUS "WITH_CASTOR=${WITH_CASTOR}")
if (WITH_CASTOR)
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
  add_subdirectory(castor)
  add_definitions(-DWITH_CASTOR)
endif()

add_subdirectory(auth)
add_subdirectory(metrics)
add_subdirectory(server)

add_subdirectory(ticketing)

option(BUILD_SERVERS "Build the pep servers" ON)
message(STATUS "BUILD_SERVERS=${BUILD_SERVERS}")

add_subdirectory(application)
if (BUILD_SERVERS)
  add_subdirectory(service-application)
endif()

add_subdirectory(transcryptor)
add_subdirectory(accessmanager) # depends on transcryptor
add_subdirectory(storagefacility)

add_subdirectory(core-client)

if(WITH_BENCHMARK)
  add_subdirectory(benchmark)
endif()

add_subdirectory(keyserver)
add_subdirectory(authserver)
add_subdirectory(registrationserver)

add_subdirectory(content)
add_subdirectory(client)

if (BUILD_SERVERS)
  add_subdirectory(servers) # Unit test requires full client
endif()

add_subdirectory(cli)
add_subdirectory(apps)
add_subdirectory(logon)
if (WITH_CASTOR)
  add_subdirectory(pullcastor)
endif()

option(BUILD_CLIENT "Build GUI software" ON)
message(STATUS "BUILD_CLIENT=${BUILD_CLIENT}")

if (BUILD_CLIENT)
  add_subdirectory(gui)
  add_subdirectory(elevate)
  add_subdirectory(assessor)
endif()
