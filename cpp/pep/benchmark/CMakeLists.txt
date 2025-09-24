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
