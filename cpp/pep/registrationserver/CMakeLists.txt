add_library(${PROJECT_NAME}RegistrationServerApilib
    RegistrationServerMessages.hpp
    RegistrationServerSerializers.cpp RegistrationServerSerializers.hpp
)
target_link_libraries(${PROJECT_NAME}RegistrationServerApilib
  ${PROJECT_NAME}RskPeplib
)
verify_headers_standalone(${PROJECT_NAME}RegistrationServerApilib)

if (WITH_SERVERS)
  add_library(${PROJECT_NAME}RegistrationServerlib
      RegistrationServer.cpp RegistrationServer.hpp
  )
  find_package(SQLite3 REQUIRED)
  target_link_libraries(${PROJECT_NAME}RegistrationServerlib
    ${PROJECT_NAME}RegistrationServerApilib
    ${PROJECT_NAME}CoreClientlib
    ${PROJECT_NAME}Serverlib
    SQLite::SQLite3
  )
  if(WITH_CASTOR)
      target_link_libraries(${PROJECT_NAME}RegistrationServerlib ${PROJECT_NAME}Castorlib)
  endif()
  verify_headers_standalone(${PROJECT_NAME}RegistrationServerlib)

  add_cli_install_executable(${PROJECT_NAME}RegistrationServer Main.cpp)
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME}RegistrationServer PRIVATE VersionInfo.rc)
  endif()
  target_link_libraries(${PROJECT_NAME}RegistrationServer
    ${PROJECT_NAME}RegistrationServerlib
    ${PROJECT_NAME}ServiceApplicationlib
  )

  if(WITH_CASTOR)
      target_compile_definitions(${PROJECT_NAME}RegistrationServerlib PUBLIC WITH_CASTOR)
      if(DEFINED CASTOR_API_KEY_DIR)
          configure_executable_companion("${CASTOR_API_KEY_DIR}/CastorAPIKey.json")
      endif()
  endif()

  configure_service_debugging(RegistrationServer)

  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/registrationserver/RegistrationServer.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/registrationserver/RegistrationServerKeys.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/ShadowAdministration.pub)
  configure_executable_config_version(${PROJECT_NAME}RegistrationServer local)

  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(
      TARGET
          registrationserver_pki
      FILES
          rootCA.cert
          PEPRegistrationServer.key PEPRegistrationServer.chain
          TLSRegistrationServer.key TLSRegistrationServer.chain
  )
endif()
