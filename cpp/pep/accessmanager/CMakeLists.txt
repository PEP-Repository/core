add_library(${PROJECT_NAME}AccessManagerApilib
  AmaMessages.cpp AmaMessages.hpp
  AmaSerializers.cpp AmaSerializers.hpp
  AccessManagerMessages.cpp AccessManagerMessages.hpp
  AccessManagerSerializers.cpp AccessManagerSerializers.hpp
)
find_package(Boost REQUIRED COMPONENTS
  random
)
target_link_libraries(${PROJECT_NAME}AccessManagerApilib
  ${PROJECT_NAME}Structurelib
  ${PROJECT_NAME}TranscryptorApilib
  Boost::random
)

if (BUILD_SERVERS)
  find_package(SqliteOrm REQUIRED)

  add_library(${PROJECT_NAME}AccessManagerlib
      AccessManager.cpp AccessManager.hpp
      Backend.cpp Backend.hpp
      Storage.cpp Storage.hpp
      Records.cpp Records.hpp
  )

  target_link_libraries(${PROJECT_NAME}AccessManagerlib
    ${PROJECT_NAME}AccessManagerApilib
    ${PROJECT_NAME}Serverlib
    sqlite_orm::sqlite_orm
  )

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
