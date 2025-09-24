# Library providing versioning.

add_library(${PROJECT_NAME}Versioninglib STATIC
    GitlabVersion.cpp GitlabVersion.hpp
    SemanticVersion.cpp SemanticVersion.hpp
    Version.cpp Version.hpp
)

target_link_libraries(${PROJECT_NAME}Versioninglib
  ${PROJECT_NAME}Protolib # Allow generated header(s) to be found
  ${PROJECT_NAME}Utilslib
)
add_dependencies(${PROJECT_NAME}Versioninglib
  ${PROJECT_NAME}Protohash  # Generate Messages.pb.checksum.h
)

add_unit_tests(Versioning)
verify_headers_standalone(${PROJECT_NAME}Versioninglib)
