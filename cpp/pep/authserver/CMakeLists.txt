add_library(${PROJECT_NAME}AuthserverApilib
  AsaMessages.hpp
  AsaSerializers.cpp AsaSerializers.hpp
)
target_link_libraries(${PROJECT_NAME}AuthserverApilib
  ${PROJECT_NAME}Cryptolib
)

if (BUILD_SERVERS)
  option(ENABLE_OAUTH_TEST_USERS "Show a list of test users for OAuth when opening the authorization endpoint in your browser." ON)

  find_package(SqliteOrm REQUIRED)

  add_library(${PROJECT_NAME}Authserverlib
      OAuthProvider.cpp OAuthProvider.hpp
      AuthserverStorage.cpp AuthserverStorage.hpp
      AuthserverBackend.cpp AuthserverBackend.hpp
      Authserver.cpp Authserver.hpp
  )
  target_link_libraries(${PROJECT_NAME}Authserverlib
    ${PROJECT_NAME}Serverlib
    ${PROJECT_NAME}Httpserverlib
    ${PROJECT_NAME}AuthserverApilib
    ${PROJECT_NAME}Templatinglib
    sqlite_orm::sqlite_orm
  )

  if (ENABLE_OAUTH_TEST_USERS)
    target_compile_definitions(${PROJECT_NAME}Authserverlib PRIVATE ENABLE_OAUTH_TEST_USERS)
  endif()

  add_cli_install_executable(${PROJECT_NAME}Authserver Main.cpp)
  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME}Authserver PRIVATE VersionInfo.rc)
  endif()
  target_link_libraries(${PROJECT_NAME}Authserver
    ${PROJECT_NAME}Authserverlib
    ${PROJECT_NAME}ServiceApplicationlib
  )

  configure_service_debugging(Authserver)

  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/authserver/Authserver.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/../OAuthTokenSecret.json OPTIONAL)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/authserver/AuthserverHTTPSCertificate.pem OPTIONAL)
  configure_executable_companions_to_subdir_from("templates/authserver" "${CMAKE_SOURCE_DIR}/web/templates/authserver")
  configure_executable_companions_to_subdir_from("templates/common" "${CMAKE_SOURCE_DIR}/web/templates/common")
  configure_executable_config_version(${PROJECT_NAME}Authserver local)

  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(
      TARGET
          authserver_pki
      FILES
          rootCA.cert
          PEPAuthserver.key PEPAuthserver.chain
          TLSAuthserver.key TLSAuthserver.chain
  )

  add_unit_tests(Authserver)
endif()
