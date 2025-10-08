add_library(${PROJECT_NAME}KeyServerApilib
  tokenblocking/BlocklistEntry.hpp
  tokenblocking/TokenIdentifier.hpp
  KeyServerMessages.hpp
  KeyServerSerializers.cpp KeyServerSerializers.hpp
)
target_link_libraries(${PROJECT_NAME}KeyServerApilib
  ${PROJECT_NAME}Serializationlib
)
verify_headers_standalone(${PROJECT_NAME}KeyServerApilib)


add_library(${PROJECT_NAME}KeyServerProxylib
  KeyServerProxy.cpp KeyServerProxy.hpp
)
target_link_libraries(${PROJECT_NAME}KeyServerProxylib
  ${PROJECT_NAME}KeyServerApilib
  ${PROJECT_NAME}ServerProxylib
)
verify_headers_standalone(${PROJECT_NAME}KeyServerProxylib)


if (WITH_SERVERS)
  add_library(${PROJECT_NAME}KeyServerlib
    KeyServer.cpp KeyServer.hpp
    tokenblocking/Blocklist.hpp
    tokenblocking/SqliteBlocklist.cpp tokenblocking/SqliteBlocklist.hpp
  )
  target_link_libraries(${PROJECT_NAME}KeyServerlib
    ${PROJECT_NAME}Serverlib
    ${PROJECT_NAME}KeyServerApilib
    ${PROJECT_NAME}Databaselib
  )
  verify_headers_standalone(${PROJECT_NAME}KeyServerlib)

  add_cli_install_executable(${PROJECT_NAME}KeyServer Main.cpp)
  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME}KeyServer PRIVATE VersionInfo.rc)
  endif()
  target_link_libraries(${PROJECT_NAME}KeyServer
    ${PROJECT_NAME}KeyServerlib
    ${PROJECT_NAME}ServiceApplicationlib
  )

  configure_service_debugging(KeyServer)

  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/keyserver/KeyServer.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/../OAuthTokenSecret.json)
  configure_executable_config_version(${PROJECT_NAME}KeyServer local)

  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(
      TARGET
          keyserver_pki
      FILES
          rootCA.cert
          pepClientCA.key pepClientCA.chain
          TLSKeyServer.key TLSKeyServer.chain
  )

  add_unit_tests(KeyServer)
endif()
