# Functionality for PEP servers.

add_library(${PROJECT_NAME}ServerApilib
  MonitoringMessages.hpp
  MonitoringSerializers.cpp MonitoringSerializers.hpp
  ServerProxy.cpp ServerProxy.hpp
  SigningServerProxy.cpp SigningServerProxy.hpp
)
target_link_libraries(${PROJECT_NAME}ServerApilib
  ${PROJECT_NAME}Messaginglib
)
verify_headers_standalone(${PROJECT_NAME}ServerApilib)


if (WITH_SERVERS)
  add_library(${PROJECT_NAME}Serverlib STATIC
    NetworkedServer.cpp NetworkedServer.hpp
    Server.cpp Server.hpp
    SigningServer.cpp SigningServer.hpp
  )
  target_link_libraries(${PROJECT_NAME}Serverlib
    ${PROJECT_NAME}Messaginglib
    ${PROJECT_NAME}Rsklib
    ${PROJECT_NAME}Authlib
    ${PROJECT_NAME}Metricslib
    ${PROJECT_NAME}ServerApilib
  )
  verify_headers_standalone(${PROJECT_NAME}Serverlib)
endif()
