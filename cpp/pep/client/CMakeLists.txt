# Library providing client connectivity to (a full constellation of) PEP servers.

add_library(${PROJECT_NAME}Clientlib STATIC
    Client.cpp Client.hpp Client_fwd.hpp
    Client.TokenBlock.cpp
)

target_link_libraries(${PROJECT_NAME}Clientlib
  ${PROJECT_NAME}CoreClientlib
  ${PROJECT_NAME}Contentlib
  ${PROJECT_NAME}KeyServerApilib
  ${PROJECT_NAME}AuthserverApilib
  ${PROJECT_NAME}RegistrationServerApilib
)

verify_headers_standalone(${PROJECT_NAME}Clientlib)
