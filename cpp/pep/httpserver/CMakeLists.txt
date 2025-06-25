# Library providing an HTTP server class.

add_library(${PROJECT_NAME}Httpserverlib
  HTTPServer.cpp HTTPServer.hpp
)

find_package(civetweb REQUIRED)
target_link_libraries(${PROJECT_NAME}Httpserverlib
PUBLIC
  ${PROJECT_NAME}Networkinglib
PRIVATE
  civetweb::civetweb
)
verify_headers_standalone(${PROJECT_NAME}Httpserverlib)
