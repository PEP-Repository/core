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

find_package(Boost REQUIRED)
find_package(OpenSSL 3.0 REQUIRED COMPONENTS
  Crypto
)

target_link_libraries(${PROJECT_NAME}Cryptolib
  ${PROJECT_NAME}Serializationlib
  Boost::date_time
  OpenSSL::Crypto
)

add_unit_tests(Crypto)
verify_headers_standalone(${PROJECT_NAME}Cryptolib)
