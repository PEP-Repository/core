set(CMAKE_AUTOMOC ON)

include("../ProvideQt.cmake")

add_library(${PROJECT_NAME}Guilib
  InterProcess.cpp InterProcess.hpp
  PlaintextCredentials.cpp PlaintextCredentials.hpp
  QTrxGui.cpp QTrxGui.hpp
)
target_link_libraries(${PROJECT_NAME}Guilib
  ${PROJECT_NAME}Utilslib
  Qt6::Core
  Qt6::PrintSupport
  Qt6::Svg
  Qt6::Widgets
  rxcpp::rxcpp
)
verify_headers_standalone(${PROJECT_NAME}Guilib)
