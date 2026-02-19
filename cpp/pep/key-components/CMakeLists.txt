add_library(${PROJECT_NAME}KeyComponentApilib
  KeyComponentMessages.cpp KeyComponentMessages.hpp
  KeyComponentSerializers.cpp KeyComponentSerializers.hpp
  EnrollmentServerProxy.cpp EnrollmentServerProxy.hpp
)

target_link_libraries(${PROJECT_NAME}KeyComponentApilib
  ${PROJECT_NAME}Morphinglib
  ${PROJECT_NAME}ServerApilib
)
verify_headers_standalone(${PROJECT_NAME}KeyComponentApilib)


if (WITH_SERVERS)
  add_library(${PROJECT_NAME}KeyComponentServerlib
    EnrollmentServer.cpp EnrollmentServer.hpp
  )

  target_link_libraries(${PROJECT_NAME}KeyComponentServerlib
    ${PROJECT_NAME}KeyComponentApilib
    ${PROJECT_NAME}Serverlib
  )

  verify_headers_standalone(${PROJECT_NAME}KeyComponentServerlib)
endif()
