# Functionality for PEP servers.

add_library(${PROJECT_NAME}ServerApilib
  MonitoringMessages.hpp
  MonitoringSerializers.cpp MonitoringSerializers.hpp
)
target_link_libraries(${PROJECT_NAME}ServerApilib
  ${PROJECT_NAME}Cryptolib
  ${PROJECT_NAME}Serializationlib
)
verify_headers_standalone(${PROJECT_NAME}ServerApilib)

if (WITH_SERVERS)
  add_library(${PROJECT_NAME}Serverlib STATIC
    MonitorableServer.cpp MonitorableServer.hpp
    Server.hpp
  )
  target_link_libraries(${PROJECT_NAME}Serverlib
    ${PROJECT_NAME}Networkinglib
    ${PROJECT_NAME}Rsklib
    ${PROJECT_NAME}Authlib
    ${PROJECT_NAME}Metricslib
    ${PROJECT_NAME}ServerApilib
  )
  verify_headers_standalone(${PROJECT_NAME}Serverlib)
endif()
