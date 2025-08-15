# Library implementing PEP RSK operations not specific to PEP repo.


add_library(${PROJECT_NAME}RskPeplib STATIC
  DataTranslationKeys.hpp
  DataTranslator.cpp DataTranslator.hpp
  KeyDomain.hpp
  PseudonymTranslationKeys.hpp
  PseudonymTranslator.cpp PseudonymTranslator.hpp
  Pseudonyms.cpp Pseudonyms.hpp
  Pseudonyms.PropertySerializers.cpp Pseudonyms.PropertySerializers.hpp
)

target_link_libraries(${PROJECT_NAME}RskPeplib
  ${PROJECT_NAME}Rsklib
)

add_unit_tests(RskPep)
verify_headers_standalone(${PROJECT_NAME}RskPeplib)
