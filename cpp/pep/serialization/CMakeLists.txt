# Library providing the general serialization mechanism, and the (network-portable) Error type.

add_library(${PROJECT_NAME}Serializationlib STATIC
    Error.cpp Error.hpp
    ErrorSerializer.cpp ErrorSerializer.hpp
    IndexList.hpp # Defined in this library to make this simple utility type widely available
    IndexListSerializer.cpp IndexListSerializer.hpp
    MessageMagic.cpp MessageMagic.hpp
    MessageSerializer.hpp
    NormalizedTypeNaming.cpp NormalizedTypeNaming.hpp
    ProtocolBufferedSerializer.hpp
    Serialization.hpp
    SerializeException.hpp
    Serializer.hpp
)

target_link_libraries(${PROJECT_NAME}Serializationlib
  ${PROJECT_NAME}Protolib
  ${PROJECT_NAME}Utilslib
)

add_unit_tests(Serialization)
verify_headers_standalone(${PROJECT_NAME}Serializationlib)
