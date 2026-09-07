include(FetchContent)
FetchContent_Declare(
  rxcpp
  GIT_REPOSITORY https://gitlab.pep.cs.ru.nl/mirror/RxCpp.git
  GIT_TAG        61f4d854b0b59eaf61d4adb4ed069f78e6c0eb14
  GIT_SUBMODULES ""
  SOURCE_DIR     ../../cache/rxcpp
  EXCLUDE_FROM_ALL
  SYSTEM
)
FetchContent_MakeAvailable(rxcpp)
add_library(rxcpp::rxcpp ALIAS rxcpp)
