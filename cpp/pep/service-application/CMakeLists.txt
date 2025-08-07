# Library providing boilerplate for applications hosting (a) PEP service(s).

add_library(${PROJECT_NAME}ServiceApplicationlib STATIC
    Service.cpp Service.hpp
    ServiceApplication.cpp ServiceApplication.hpp
)

target_link_libraries(${PROJECT_NAME}ServiceApplicationlib
  ${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Networkinglib
)
verify_headers_standalone(${PROJECT_NAME}ServiceApplicationlib)
