cmake_minimum_required(VERSION 3.28)

include(FetchContent)
FetchContent_Declare(
  rxcpp
  GIT_REPOSITORY https://gitlab.pep.cs.ru.nl/mirror/RxCpp.git
  GIT_TAG        89c36676e75e1ca8a44b022b614c11fd7d440e2d
  GIT_SUBMODULES ""
  SOURCE_DIR     ../../cache/rxcpp
  EXCLUDE_FROM_ALL
  SYSTEM
)
FetchContent_MakeAvailable(rxcpp)
