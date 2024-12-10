find_package(inja REQUIRED)

add_library(${PROJECT_NAME}Templatinglib
    TemplateEnvironment.cpp TemplateEnvironment.hpp
)

target_link_libraries(${PROJECT_NAME}Templatinglib
  pantor::inja
)