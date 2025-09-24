# Functionality related to predefined content (formats) stored in PEP.

add_library(${PROJECT_NAME}Contentlib STATIC
  Date.cpp Date.hpp
  ParticipantDeviceHistory.cpp ParticipantDeviceHistory.hpp
  ParticipantPersonalia.cpp ParticipantPersonalia.hpp
)

find_package(Boost REQUIRED)
target_link_libraries(${PROJECT_NAME}Contentlib
  Boost::date_time
  Boost::headers
)

verify_headers_standalone(${PROJECT_NAME}Contentlib)
add_unit_tests(Content)
