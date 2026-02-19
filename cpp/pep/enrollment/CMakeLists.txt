add_library(${PROJECT_NAME}EnrollmentApilib
  KeyComponentMessages.cpp KeyComponentMessages.hpp
  KeyComponentSerializers.cpp KeyComponentSerializers.hpp
  EnrollmentServerProxy.cpp EnrollmentServerProxy.hpp
)

target_link_libraries(${PROJECT_NAME}EnrollmentApilib
  ${PROJECT_NAME}Morphinglib
  ${PROJECT_NAME}ServerApilib
)
verify_headers_standalone(${PROJECT_NAME}EnrollmentApilib)


if (WITH_SERVERS)
  add_library(${PROJECT_NAME}EnrollmentServerlib
    EnrollmentServer.cpp EnrollmentServer.hpp
  )

  target_link_libraries(${PROJECT_NAME}EnrollmentServerlib
    ${PROJECT_NAME}EnrollmentApilib
    ${PROJECT_NAME}Serverlib
  )

  verify_headers_standalone(${PROJECT_NAME}EnrollmentServerlib)
endif()
