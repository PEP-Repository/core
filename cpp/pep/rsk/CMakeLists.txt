# Library implementing RSK operations not specific to PEP.


add_library(${PROJECT_NAME}Rsklib STATIC
  EGCache.cpp EGCache.hpp
  Proofs.cpp Proofs.hpp
  RskKeys.cpp RskKeys.hpp
  RskRecipient.cpp RskRecipient.hpp
  RskSerializers.cpp RskSerializers.hpp
  RskTranslator.cpp RskTranslator.hpp
  Verifiers.cpp Verifiers.hpp  #TODO move to other lib?
)

target_link_libraries(${PROJECT_NAME}Rsklib
  ${PROJECT_NAME}Elgamallib
)

add_unit_tests(Rsk)
verify_headers_standalone(${PROJECT_NAME}Rsklib)
