# Library providing an HTTP server class and related functionality.

find_package(civetweb REQUIRED)

add_library(${PROJECT_NAME}Httpserverlib
        HTTPServer.cpp HTTPServer.hpp
        OAuthClient.cpp OAuthClient.hpp
)
target_link_libraries(${PROJECT_NAME}Httpserverlib
  ${PROJECT_NAME}Networkinglib
  civetweb
  civetweb::civetweb-cpp
)
