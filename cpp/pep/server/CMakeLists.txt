# Functionality for PEP servers.

add_library(${PROJECT_NAME}Serverlib STATIC
  MonitorableServer.cpp MonitorableServer.hpp
  MonitoringMessages.hpp
  MonitoringSerializers.cpp MonitoringSerializers.hpp
  Server.hpp
)
find_package(prometheus-cpp REQUIRED COMPONENTS
  push
)
target_link_libraries(${PROJECT_NAME}Serverlib
  ${PROJECT_NAME}Networkinglib
  ${PROJECT_NAME}Rsklib
  ${PROJECT_NAME}Authlib
  ${PROJECT_NAME}Metricslib
)
