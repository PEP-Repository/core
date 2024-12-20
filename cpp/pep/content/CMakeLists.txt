# Functionality related to predefined content (formats) stored in PEP.

add_library(${PROJECT_NAME}Contentlib STATIC
  Date.cpp Date.hpp
  ParticipantDeviceHistory.cpp ParticipantDeviceHistory.hpp
  ParticipantPersonalia.cpp ParticipantPersonalia.hpp
)
find_package(Boost REQUIRED COMPONENTS
  headers
)
target_link_libraries(${PROJECT_NAME}Contentlib
  Boost::headers
)

add_unit_tests(Content)
