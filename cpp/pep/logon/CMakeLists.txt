add_cli_install_executable(${PROJECT_NAME}Logon Logon.cpp)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}Logon PRIVATE Logon.VersionInfo.rc)
endif()
target_link_libraries(${PROJECT_NAME}Logon
  ${PROJECT_NAME}Clientlib
  ${PROJECT_NAME}OAuthClientlib
  ${PROJECT_NAME}Applicationlib
)
configure_debug_command_line_parameter(${PROJECT_NAME}Logon ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/ClientConfig.json)

configure_executable_companion(${PEP_INFRA_DIR}/client/ClientConfig.json REPLACE_VARIABLES)
configure_executable_companion(${PEP_INFRA_DIR}/rootCA.cert OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/ShadowAdministration.pub OPTIONAL)
configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/authserver/AuthserverHTTPSCertificate.pem OPTIONAL)
configure_executable_config_version(${PROJECT_NAME}Logon infra)

# Only copy generated PKI file if predefined one does not exist for this infrastructure
if(NOT EXISTS "${PEP_INFRA_DIR}/rootCA.cert")
  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(TARGET logon_pki FILES rootCA.cert)
  add_dependencies(${PROJECT_NAME}Logon logon_pki)
endif()
