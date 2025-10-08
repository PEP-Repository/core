add_library(${PROJECT_NAME}AuthserverApilib
  AuthserverMessages.hpp
  AuthserverSerializers.cpp AuthserverSerializers.hpp
)
target_link_libraries(${PROJECT_NAME}AuthserverApilib
  ${PROJECT_NAME}Cryptolib
)
verify_headers_standalone(${PROJECT_NAME}AuthserverApilib)


add_library(${PROJECT_NAME}AuthClientlib
  AuthClient.cpp AuthClient.hpp
)
target_link_libraries(${PROJECT_NAME}AuthClientlib
  ${PROJECT_NAME}AuthserverApilib
  ${PROJECT_NAME}ServerProxylib
)
verify_headers_standalone(${PROJECT_NAME}AuthClientlib)


if (WITH_SERVERS)
  add_library(${PROJECT_NAME}Authserverlib
    OAuthProvider.cpp OAuthProvider.hpp
    AuthserverBackend.cpp AuthserverBackend.hpp
    Authserver.cpp Authserver.hpp
  )
  target_link_libraries(${PROJECT_NAME}Authserverlib
    ${PROJECT_NAME}Serverlib
    ${PROJECT_NAME}Httpserverlib
    ${PROJECT_NAME}AuthserverApilib
    ${PROJECT_NAME}AccessManagerClientlib
    ${PROJECT_NAME}Templatinglib
  )
  verify_headers_standalone(${PROJECT_NAME}Authserverlib)

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
endif()
