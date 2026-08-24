include(FetchContent)
FetchContent_Declare(
  rxcpp
  GIT_REPOSITORY https://gitlab.pep.cs.ru.nl/mirror/RxCpp.git
  GIT_TAG        91722d3601da2c5de9040c94eb5fde55e55d4f9b
  GIT_SUBMODULES ""
  SOURCE_DIR     ../../cache/rxcpp
  EXCLUDE_FROM_ALL
  SYSTEM
)
FetchContent_MakeAvailable(rxcpp)
add_library(rxcpp::rxcpp ALIAS rxcpp)
