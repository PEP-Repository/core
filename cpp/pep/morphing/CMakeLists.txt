# Library implementing PEP RSK for PEP repo.


add_library(${PROJECT_NAME}Morphinglib STATIC
  Metadata.hpp Metadata.cpp # TODO: move to different lib?
  MorphingSerializers.cpp MorphingSerializers.hpp
  RepoKeys.cpp RepoKeys.hpp
  RepoRecipient.cpp RepoRecipient.hpp
)

target_link_libraries(${PROJECT_NAME}Morphinglib
  ${PROJECT_NAME}Authlib
  ${PROJECT_NAME}RskPeplib
)

add_unit_tests(Morphing)
verify_headers_standalone(${PROJECT_NAME}Morphinglib)
