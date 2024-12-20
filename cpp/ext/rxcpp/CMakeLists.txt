include(FetchContent)
FetchContent_Declare(
  rxcpp
  GIT_REPOSITORY https://gitlab.pep.cs.ru.nl/mirror/RxCpp.git
  GIT_TAG        54de6141d2afa1f948777949e0b45e2f200ae539
  GIT_SUBMODULES ""
  SOURCE_DIR     ../../cache/rxcpp
)

#XXX Workaround: in CMake 3.28 we can add EXCLUDE_FROM_ALL to FetchContent_Declare and use FetchContent_MakeAvailable
FetchContent_GetProperties(rxcpp)
if (NOT rxcpp_populated)
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD) # This FetchContent_Populate is deprecated, see workaround comments; https://cmake.org/cmake/help/latest/policy/CMP0169.html
  endif()
  FetchContent_Populate(rxcpp)
  # EXCLUDE_FROM_ALL prevents headers from being installed
  add_subdirectory(${rxcpp_SOURCE_DIR} ${rxcpp_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

#XXX Workaround: in CMake 3.24 we can add SYSTEM to FetchContent_Declare
get_target_property(RXCPP_IID rxcpp INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(rxcpp PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${RXCPP_IID}")
