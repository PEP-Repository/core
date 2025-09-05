# Library providing networking functionality based on Boost ASIO.

set(SSLKEYLOGFILE "" CACHE STRING "SSL Keylogfile, to be used with Wireshark")
if(SSLKEYLOGFILE)
  add_definitions(-DKEYLOG_FILE="${SSLKEYLOGFILE}")
endif()

add_library(${PROJECT_NAME}Networkinglib STATIC
    CertificateVerification.cpp CertificateVerification.hpp
    Client.cpp Client.hpp
    Connection.cpp Connection.hpp
    ConnectivityAttempt.hpp
    EndPoint.hpp
    EndPoint.PropertySerializer.cpp EndPoint.PropertySerializer.hpp
    ExponentialBackoff.cpp ExponentialBackoff.hpp
    HttpClient.cpp HttpClient.hpp
    HTTPMessage.cpp HTTPMessage.hpp
    HttpMethod.cpp HttpMethod.hpp
    InstanceBound.hpp
    Node.cpp Node.hpp
    Protocol.hpp
    Server.cpp Server.hpp
    SocketReadBuffer.cpp SocketReadBuffer.hpp
    SslError.cpp SslError.hpp
    StreamSocket.hpp
    Tcp.cpp Tcp.hpp
    TcpBasedProtocol.cpp TcpBasedProtocol.hpp
    Tls.cpp Tls.hpp
    Transport.cpp Transport.hpp
)

find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS
  SSL
)
target_link_libraries(${PROJECT_NAME}Networkinglib
  ${PROJECT_NAME}Asynclib
  ${PROJECT_NAME}Cryptolib
  Boost::url
  OpenSSL::SSL
)

add_unit_tests(Networking LINK_LIBS pepHttpserverlib)
verify_headers_standalone(${PROJECT_NAME}Networkinglib)
