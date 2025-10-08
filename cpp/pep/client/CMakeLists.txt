# Library providing client connectivity to (a full constellation of) PEP servers.

add_library(${PROJECT_NAME}Clientlib STATIC
    Client.cpp Client.hpp Client_fwd.hpp
)

target_link_libraries(${PROJECT_NAME}Clientlib
  ${PROJECT_NAME}AuthServerProxylib
  ${PROJECT_NAME}CoreClientlib
  ${PROJECT_NAME}Contentlib
  ${PROJECT_NAME}KeyServerProxylib
  ${PROJECT_NAME}RegistrationServerProxylib
)

verify_headers_standalone(${PROJECT_NAME}Clientlib)
