add_library(${PROJECT_NAME}AccessManagerApilib
  AmaMessages.cpp AmaMessages.hpp
  AmaSerializers.cpp AmaSerializers.hpp
  UserMessages.hpp
  UserSerializers.cpp UserSerializers.cpp
  AccessManagerMessages.cpp AccessManagerMessages.hpp
  AccessManagerSerializers.cpp AccessManagerSerializers.hpp
)
target_link_libraries(${PROJECT_NAME}AccessManagerApilib
  ${PROJECT_NAME}Structurelib
  ${PROJECT_NAME}TranscryptorApilib
  ${PROJECT_NAME}KeyServerApilib
)
verify_headers_standalone(${PROJECT_NAME}AccessManagerApilib)


add_library(${PROJECT_NAME}AccessManagerClientlib
  AccessManagerClient.cpp AccessManagerClient.hpp
)
target_link_libraries(${PROJECT_NAME}AccessManagerClientlib
  ${PROJECT_NAME}AccessManagerApilib
  ${PROJECT_NAME}ServerProxylib
)
verify_headers_standalone(${PROJECT_NAME}AccessManagerClientlib)


if (WITH_SERVERS)
  add_library(${PROJECT_NAME}AccessManagerlib
      AccessManager.cpp AccessManager.hpp
      Backend.cpp Backend.hpp
      Storage.cpp Storage.hpp
      Records.cpp Records.hpp
      LegacyAuthserverStorage.cpp LegacyAuthserverStorage.hpp
      UserStorageRecords.cpp UserStorageRecords.hpp
  )

  target_link_libraries(${PROJECT_NAME}AccessManagerlib
    ${PROJECT_NAME}AccessManagerApilib
    ${PROJECT_NAME}Databaselib
    ${PROJECT_NAME}KeyClientlib
    ${PROJECT_NAME}Serverlib
    ${PROJECT_NAME}TranscryptorClientlib
  )

  option(AUTO_POPULATE_USER_DB "Should the user DB be automatically populated, when storage is empty?" ON)
  if(AUTO_POPULATE_USER_DB)
    target_compile_definitions(${PROJECT_NAME}AccessManagerlib PRIVATE AUTO_POPULATE_USER_DB)
  endif()

  verify_headers_standalone(${PROJECT_NAME}AccessManagerlib) # TODO: use same target_compile_definitions (e.g. AUTO_POPULATE_USER_DB)

  add_cli_install_executable(${PROJECT_NAME}AccessManager Main.cpp)
  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME}AccessManager PRIVATE VersionInfo.rc)
  endif()
  target_link_libraries(${PROJECT_NAME}AccessManager
    ${PROJECT_NAME}AccessManagerlib
    ${PROJECT_NAME}ServiceApplicationlib
  )

  configure_service_debugging(AccessManager)

  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/accessmanager/AccessManager.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/accessmanager/AccessManagerKeys.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/accessmanager/SystemKeys.json)
  configure_executable_companion(${PEP_PROJECT_DIR}/accessmanager/GlobalConfiguration.json)
  configure_executable_config_version(${PROJECT_NAME}AccessManager local)

  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(
      TARGET
          accessmanager_pki
      FILES
          rootCA.cert
          PEPAccessManager.key PEPAccessManager.chain
          TLSAccessManager.key TLSAccessManager.chain
  )

  add_unit_tests(AccessManager)
endif()
