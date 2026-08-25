include(FetchContent)
FetchContent_Declare(
  rxcpp
  GIT_REPOSITORY https://gitlab.pep.cs.ru.nl/mirror/RxCpp.git
  GIT_TAG        337cd6cfc619b14c8ea2aaf49c1714e6b2e7aee6
  GIT_SUBMODULES ""
  SOURCE_DIR     ../../cache/rxcpp
  EXCLUDE_FROM_ALL
  SYSTEM
)
FetchContent_MakeAvailable(rxcpp)
add_library(rxcpp::rxcpp ALIAS rxcpp)
