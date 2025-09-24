add_library(${PROJECT_NAME}Templatinglib
    TemplateEnvironment.cpp TemplateEnvironment.hpp
)

find_package(inja REQUIRED)
target_link_libraries(${PROJECT_NAME}Templatinglib
  pantor::inja
)
verify_headers_standalone(${PROJECT_NAME}Templatinglib)
