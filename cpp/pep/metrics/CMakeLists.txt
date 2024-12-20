# Library providing boilerplate for Prometheus-based metrics.

find_package(prometheus-cpp REQUIRED COMPONENTS
  push
)

add_library(${PROJECT_NAME}Metricslib
  RegisteredMetrics.cpp RegisteredMetrics.hpp
)
target_link_libraries(${PROJECT_NAME}Metricslib
  prometheus-cpp::push
)
