# Library providing boilerplate for Prometheus-based metrics.

add_library(${PROJECT_NAME}Metricslib
  RegisteredMetrics.cpp RegisteredMetrics.hpp
)

find_package(prometheus-cpp REQUIRED COMPONENTS
  push
)
target_link_libraries(${PROJECT_NAME}Metricslib
  prometheus-cpp::push
)
verify_headers_standalone(${PROJECT_NAME}Metricslib)
