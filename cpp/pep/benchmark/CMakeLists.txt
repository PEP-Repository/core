add_executable(${PROJECT_NAME}benchmark benchmark.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}benchmark PRIVATE benchmark.VersionInfo.rc)
  add_windows_manifest(${PROJECT_NAME}benchmark)
endif()

find_package(benchmark REQUIRED)
target_link_libraries(${PROJECT_NAME}benchmark
  ${PROJECT_NAME}AccessManagerApilib
  ${PROJECT_NAME}StorageFacilityApilib
  benchmark::benchmark
)

# For Clang >=22: Silence warning about __COUNTER__, which now apparently is a C2y extension
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 22)
  target_compile_options(${PROJECT_NAME}benchmark PRIVATE -Wno-c2y-extensions)
endif()
