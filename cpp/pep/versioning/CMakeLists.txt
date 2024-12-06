# Library providing versioning.

add_library(${PROJECT_NAME}Versioninglib STATIC
    GitlabVersion.cpp GitlabVersion.hpp
    Version.cpp Version.hpp
)

target_link_libraries(${PROJECT_NAME}Versioninglib
  ${PROJECT_NAME}Protolib # Allow generated header(s) to be found
  ${PROJECT_NAME}Utilslib
)
add_dependencies(${PROJECT_NAME}Versioninglib
  ${PROJECT_NAME}Protohash  # Generate Messages.pb.checksum.h
)
