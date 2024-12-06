# Library implementing PEP RSK operations not specific to PEP repo.


add_library(${PROJECT_NAME}RskPeplib STATIC
  DataTranslationKeys.hpp
  DataTranslator.cpp DataTranslator.hpp
  KeyDomain.hpp
  PseudonymTranslationKeys.hpp
  PseudonymTranslator.cpp PseudonymTranslator.hpp
  Pseudonyms.cpp Pseudonyms.hpp
  RskPepSerializers.cpp RskPepSerializers.hpp
)

target_link_libraries(${PROJECT_NAME}RskPeplib
  ${PROJECT_NAME}Rsklib
)

add_unit_tests(RskPep)
