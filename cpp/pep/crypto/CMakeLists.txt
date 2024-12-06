# Library providing general cryptographic functionality such as
# signing, encryption, and X509 certificates.
# For ElGamal, see the Elgamal lib.
# For PEP(repo)'s polymorphic cryptography, see the Rsk, RskPep, and Morphing libs.


add_library(${PROJECT_NAME}Cryptolib STATIC
    AsymmetricKey.cpp AsymmetricKey.hpp
    Bytes.hpp
    BytesSerializer.cpp BytesSerializer.hpp
    ConstTime.hpp
    CPRNG.cpp CPRNG.hpp
    CryptoSerializers.cpp CryptoSerializers.hpp
    Encrypted.cpp Encrypted.hpp
    GcmContext.hpp
    Signature.cpp Signature.hpp
    Signed.cpp Signed.hpp
    Timestamp.cpp Timestamp.hpp
    X509Certificate.cpp X509Certificate.hpp
)

find_package(MbedTLS 2...<3 REQUIRED COMPONENTS
  mbedx509
)

target_link_libraries(${PROJECT_NAME}Cryptolib
  ${PROJECT_NAME}Serializationlib
  MbedTLS::mbedx509
)

add_unit_tests(Crypto)
