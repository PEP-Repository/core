# Utilities library: provides general functionality not directly tied to PEP.
# Depends on no other PEP libraries (but requires some external ones).
# Provides support code for testing library (also defined in this file).

if((CMAKE_BUILD_TYPE STREQUAL "Release") OR
   (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"))
    message("Note: log statements with severity less than info are left out")
    set(DEFAULT_LOGGING info)
else()
    set(DEFAULT_LOGGING verbose)
endif()
set(LOGGING ${DEFAULT_LOGGING} CACHE STRING
        "Logging stataments to compile into binaries.  (Note: there are also runtime filters.)")
add_definitions(-DPEP_COMPILED_MIN_LOG_SEVERITY=${LOGGING})

add_library(${PROJECT_NAME}Utilslib STATIC
    ApplicationMetrics.cpp ApplicationMetrics.hpp
    Attributes.hpp
    Base64.cpp Base64.hpp
    Bitpacking.cpp Bitpacking.hpp
    BoostHexUtil.hpp
    BuildFlavor.hpp
    ChronoUtil.hpp
    CollectionUtils.hpp CollectionUtils.cpp
    Compare.hpp
    Configuration.cpp Configuration.hpp Configuration_fwd.hpp
    Defer.hpp
    Event.cpp Event.hpp
    Exceptions.cpp Exceptions.hpp
    File.cpp File.hpp
    Filesystem.cpp Filesystem.hpp
    Hasher.hpp
    InstanceTracker.hpp
    LifeCycler.cpp LifeCycler.hpp
    LocalSettings.cpp LocalSettings.hpp
    Log.cpp Log.hpp
    Md5.cpp Md5.hpp
    MiscUtil.cpp MiscUtil.hpp
    Mod97.cpp Mod97.hpp
    MultiTypeTransform.hpp
    OpensslUtils.hpp OpensslUtils.cpp
    OperationResult.hpp
    Paths.cpp Paths.hpp
    PlainTypeName.hpp PlainTypeName.cpp
    Platform.cpp Platform.hpp
    Progress.cpp Progress.hpp
    PropertyBasedContainer.hpp
    PropertySerializer.hpp
    Random.cpp Random.hpp
    Raw.cpp Raw.hpp
    SelfRegistering.hpp
    Sha.hpp
    Shared.hpp
    Singleton.hpp
    Stream.cpp Stream.hpp
    StringStream.cpp StringStream.hpp
    ThreadUtil.cpp ThreadUtil.hpp
    TypeTraits.hpp
    VectorOfVectors.hpp
    XxHasher.cpp XxHasher.hpp
)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}Utilslib PRIVATE
    Win32Api.cpp Win32Api.hpp
  )
endif()

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS
  Crypto
)
find_package(xxHash REQUIRED)

target_link_libraries(${PROJECT_NAME}Utilslib
  ${CMAKE_THREAD_LIBS_INIT}
  ${Boost_SUPPORT_LIBRARIES}
  Boost::exception
  Boost::headers
  Boost::log
  Boost::log_setup
  OpenSSL::Crypto
  xxHash::xxhash
)

verify_headers_standalone(${PROJECT_NAME}Utilslib)

if (WITH_TESTS)
  # Testing library: provides support for (unit) test executables

  add_library(${PROJECT_NAME}Testinglib STATIC
    OnFatalTestFailure.hpp
    RegisteredTestEnvironment.cpp RegisteredTestEnvironment.hpp
    TestApplicationMainFunction.cpp
    VerifyBackwardCompatibleSerialization.hpp
    TestTiming.hpp
  )
  find_package(GTest REQUIRED)
  target_link_libraries(${PROJECT_NAME}Testinglib
    ${PROJECT_NAME}Utilslib
    GTest::gtest
  )
  verify_headers_standalone(${PROJECT_NAME}Testinglib)
endif()

function(add_unit_tests libname)
  if (WITH_TESTS)
    cmake_parse_arguments(
      AUT          # Prefix for variables filled by "cmake_parse_arguments"
      ""           # This function doesn't accept any flag arguments
      "EXTRA_ARGS" # Pass all in a single string: add_unit_tests(myLib EXTRA_ARGS "one two three")
      "LINK_LIBS"  # Pass multiple in separate values: add_unit_tests(myLib LINK_LIBS libOne libTwo libThree)
      "${ARGN}"    # Parse all excess parameters (beyond those in the function signature)
    )
    if (DEFINED AUT_UNPARSED_ARGUMENTS)
      message(FATAL_ERROR "Unrecognized parameter(s) to 'add_unit_tests' function: ${AUT_UNPARSED_ARGUMENTS}")
    endif()

    file(GLOB UNITTEST_SOURCES tests/*.cpp tests/*.hpp)
    add_executable(${PROJECT_NAME}${libname}UnitTests ${UNITTEST_SOURCES})
    target_link_libraries(${PROJECT_NAME}${libname}UnitTests ${PROJECT_NAME}${libname}lib ${PROJECT_NAME}Testinglib)
    if (DEFINED AUT_LINK_LIBS)
      foreach(lib ${AUT_LINK_LIBS}) # Unquoted to prevent the semicolon-delimited list from being considered a single value
        target_link_libraries(${PROJECT_NAME}${libname}UnitTests ${lib})
      endforeach()
    endif()

    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
      target_compile_definitions(${PROJECT_NAME}${libname}UnitTests PRIVATE "PEP_EXE_FILENAME=\"pep${libname}UnitTests\"" "PEP_EXE_DESCRIPTION=\"PEP ${libname} library test executable\"")
      target_sources(${PROJECT_NAME}${libname}UnitTests PRIVATE ${PROJECT_SOURCE_DIR}/cpp/pep/Windows.Exe.VersionInfo.rc)
      add_windows_manifest(${PROJECT_NAME}${libname}UnitTests)
    endif()

    if (ENABLE_TEST_DISCOVERY)
      include(GoogleTest)
      if (DEFINED AUT_EXTRA_ARGS)
        gtest_discover_tests(${PROJECT_NAME}${libname}UnitTests DISCOVERY_TIMEOUT 60 EXTRA_ARGS "${AUT_EXTRA_ARGS}")
      else()
        gtest_discover_tests(${PROJECT_NAME}${libname}UnitTests DISCOVERY_TIMEOUT 60)
      endif()
    endif ()
  endif()
endfunction()

add_unit_tests(Utils)
