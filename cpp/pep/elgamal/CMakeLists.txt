# Library implementing ElGamal operations.


add_library(${PROJECT_NAME}Elgamallib STATIC
  CryptoAssert.cpp CryptoAssert.hpp
  CurvePoint.cpp CurvePoint.hpp
  CurvePoint.PropertySerializer.cpp CurvePoint.PropertySerializer.hpp
  CurveScalar.cpp CurveScalar.hpp
  ElgamalEncryption.cpp ElgamalEncryption.hpp
  ElgamalSerializers.cpp ElgamalSerializers.hpp
)

target_link_libraries(${PROJECT_NAME}Elgamallib
  PUBLIC ${PROJECT_NAME}Cryptolib
  PRIVATE panda
)

option(PEP_ENABLE_CRYPTO_ASSERT "Enable (expensive) crypto assertions")
if(PEP_ENABLE_CRYPTO_ASSERT)
  set_property(SOURCE CryptoAssert.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS PEP_ENABLE_CRYPTO_ASSERT)
endif()

add_unit_tests(Elgamal)
verify_headers_standalone(${PROJECT_NAME}Elgamallib)
