add_cli_install_executable(${PROJECT_NAME}Enrollment
  Enrollment.cpp Enrollment.hpp
  Enroller.cpp Enroller.hpp
)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}Enrollment PRIVATE Enrollment.VersionInfo.rc)
endif()
target_link_libraries(${PROJECT_NAME}Enrollment
  ${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Clientlib
)


add_cli_install_executable(${PROJECT_NAME}GenerateSystemKeys GenerateSystemKeys.cpp)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}GenerateSystemKeys PRIVATE GenerateSystemKeys.VersionInfo.rc)
endif()
target_link_libraries(${PROJECT_NAME}GenerateSystemKeys
  ${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Authlib
  ${PROJECT_NAME}Elgamallib
)


add_cli_install_executable(${PROJECT_NAME}Registration Registration.cpp)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}Registration PRIVATE Registration.VersionInfo.rc)
endif()
target_link_libraries(${PROJECT_NAME}Registration
  ${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Clientlib
)


find_package(SQLite3 REQUIRED)
add_cli_install_executable(${PROJECT_NAME}DumpShadowAdministration DumpShadowAdministration.cpp)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}DumpShadowAdministration PRIVATE DumpShadowAdministration.VersionInfo.rc)
endif()
target_link_libraries(${PROJECT_NAME}DumpShadowAdministration
  ${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Cryptolib
  SQLite::SQLite3
)


add_cli_install_executable(${PROJECT_NAME}ClientTest ClientTest.cpp)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}ClientTest PRIVATE ClientTest.VersionInfo.rc)
endif()
target_link_libraries(${PROJECT_NAME}ClientTest
  ${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Clientlib
)
configure_debug_command_line_parameter(${PROJECT_NAME}ClientTest ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/ClientConfig.json 5)


add_cli_install_executable(${PROJECT_NAME}Token Token.cpp)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}Token PRIVATE Token.VersionInfo.rc)
endif()
target_link_libraries(${PROJECT_NAME}Token
  ${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Authlib
)

option(RSK_PEP_TESTCASES "Build pepRskPepTestcases binary")  # Disable by default
if(RSK_PEP_TESTCASES)
  add_cli_install_executable(${PROJECT_NAME}RskPepTestcases RskPepTestcases.cpp)
  target_link_libraries(${PROJECT_NAME}RskPepTestcases
    ${PROJECT_NAME}Applicationlib
    ${PROJECT_NAME}Morphinglib
  )
endif()

configure_executable_companion(${PEP_INFRA_DIR}/client/ClientConfig.json REPLACE_VARIABLES)
configure_executable_companion(${PEP_INFRA_DIR}/rootCA.cert OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/ShadowAdministration.pub OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/../OAuthTokenSecret.json OPTIONAL)
# TODO configure_executable_config_version(Something infra) but only once

# Only copy generated PKI file if predefined one does not exist for this infrastructure
if (NOT EXISTS "${PEP_INFRA_DIR}/rootCA.cert")
  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(TARGET apps_pki FILES rootCA.cert)
  add_dependencies(${PROJECT_NAME}Enrollment apps_pki)
  add_dependencies(${PROJECT_NAME}GenerateSystemKeys apps_pki)
  add_dependencies(${PROJECT_NAME}Registration apps_pki)
  add_dependencies(${PROJECT_NAME}DumpShadowAdministration apps_pki)
  add_dependencies(${PROJECT_NAME}ClientTest apps_pki)
  add_dependencies(${PROJECT_NAME}Token apps_pki)
endif()
