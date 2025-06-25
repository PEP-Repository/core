# Library implementing ElGamal operations.


add_library(${PROJECT_NAME}Elgamallib STATIC
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

add_unit_tests(Elgamal)
verify_headers_standalone(${PROJECT_NAME}Elgamallib)
