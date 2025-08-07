# Library providing networking functionality based on Boost ASIO.

# Option to disable TLS for the network connections
#add_definitions(-DNO_TLS)

set(SSLKEYLOGFILE "" CACHE STRING "SSL Keylogfile, to be used with Wireshark")
if(SSLKEYLOGFILE)
  add_definitions(-DKEYLOG_FILE="${SSLKEYLOGFILE}")
endif()

add_library(${PROJECT_NAME}Networkinglib STATIC
    ConnectionStatus.hpp
    EndPoint.hpp
    EndPoint.PropertySerializer.cpp EndPoint.PropertySerializer.hpp
    ExponentialBackoff.hpp
    HousekeepingMessages.hpp
    HTTPMessage.cpp HTTPMessage.hpp
    HTTPSClient.cpp HTTPSClient.hpp
    MessageHeader.cpp MessageHeader.hpp
    MessageProperties.cpp MessageProperties.hpp
    MessageSequence.cpp MessageSequence.hpp
    NetworkingSerializers.cpp NetworkingSerializers.hpp
    RequestHandler.cpp RequestHandler.hpp
    Requestor.cpp Requestor.hpp
    Scheduler.cpp Scheduler.hpp
    SystemRootCAs.cpp SystemRootCAs.hpp
    TLSClient.cpp TLSClient.hpp
    TLSMessageClient.cpp TLSMessageClient.hpp
    TLSMessageProtocol.cpp TLSMessageProtocol.hpp
    TLSMessageServer.cpp TLSMessageServer.hpp
    TLSProtocol.cpp TLSProtocol.hpp
    TLSServer.cpp TLSServer.hpp
)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS
  SSL
)
target_link_libraries(${PROJECT_NAME}Networkinglib
  ${PROJECT_NAME}Asynclib
  ${PROJECT_NAME}Cryptolib
  ${PROJECT_NAME}Versioninglib
  Boost::url
  OpenSSL::SSL
)

add_unit_tests(Networking)
verify_headers_standalone(${PROJECT_NAME}Networkinglib)
