# Library providing (network) messaging functionality based on RX.

# Development helper: option to disable TLS for the network connections
option(UNENCRYPTED_MESSAGING "Use unencrypted network connections for PEP message exchange" OFF)
if(UNENCRYPTED_MESSAGING)
    add_definitions(-DNO_TLS)
endif()

add_library(${PROJECT_NAME}Messaginglib STATIC
    BinaryProtocol.cpp BinaryProtocol.hpp
    Connection.cpp Connection.hpp
    ConnectionFailureException.hpp
    ConnectionStatus.hpp
    HousekeepingMessages.cpp HousekeepingMessages.hpp
    MessageHeader.cpp MessageHeader.hpp
    MessageProperties.cpp MessageProperties.hpp
    MessageSequence.cpp MessageSequence.hpp
    MessagingSerializers.cpp MessagingSerializers.hpp
    Node.cpp Node.hpp
    RequestHandler.cpp RequestHandler.hpp
    Requestor.cpp Requestor.hpp
    ResponseToVoid.hpp
    Scheduler.cpp Scheduler.hpp
    ServerConnection.cpp ServerConnection.hpp
    Tail.hpp
)

target_link_libraries(${PROJECT_NAME}Messaginglib
  ${PROJECT_NAME}Authlib
  ${PROJECT_NAME}Networkinglib
  ${PROJECT_NAME}Versioninglib
)

add_unit_tests(Messaging)
verify_headers_standalone(${PROJECT_NAME}Messaginglib)
