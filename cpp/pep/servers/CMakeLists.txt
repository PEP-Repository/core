include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)

function(add_shared_companion_files subdir)
  if("${subdir}" STREQUAL ".")
    set(TARGET_PREFIX "servers")
  else()
    set(TARGET_PREFIX "${subdir}")
  endif()

  configure_executable_companion_to_subdir("${subdir}/accessmanager" ${PEP_LOCAL_INFRA_DIR}/accessmanager/AccessManager.json)
  configure_executable_companion_to_subdir("${subdir}/accessmanager" ${PEP_LOCAL_INFRA_DIR}/accessmanager/AccessManagerKeys.json)
  configure_executable_companion_to_subdir("${subdir}/accessmanager" ${PEP_LOCAL_INFRA_DIR}/accessmanager/SystemKeys.json)
  configure_executable_companion_to_subdir("${subdir}/accessmanager" ${PEP_PROJECT_DIR}/accessmanager/GlobalConfiguration.json)

  copy_pki_files(
    TARGET
        "${TARGET_PREFIX}_accessmanager_pki"
    SUBDIRECTORY
        "${subdir}/accessmanager"
    FILES
        rootCA.cert
        PEPAccessManager.key PEPAccessManager.chain
        TLSAccessManager.key TLSAccessManager.chain
)

  configure_executable_companion_to_subdir("${subdir}/keyserver" ${PEP_LOCAL_INFRA_DIR}/keyserver/KeyServer.json)
  configure_executable_companion_to_subdir("${subdir}/keyserver" ${PEP_LOCAL_INFRA_DIR}/../OAuthTokenSecret.json)

  copy_pki_files(
      TARGET
          "${TARGET_PREFIX}_keyserver_pki"
      SUBDIRECTORY
          "${subdir}/keyserver"
      FILES
          rootCA.cert
          pepClientCA.key pepClientCA.chain
          TLSKeyServer.key TLSKeyServer.chain
  )

  configure_executable_companion_to_subdir("${subdir}/registrationserver" ${PEP_LOCAL_INFRA_DIR}/registrationserver/RegistrationServer.json)
  configure_executable_companion_to_subdir("${subdir}/registrationserver" ${PEP_LOCAL_INFRA_DIR}/registrationserver/RegistrationServerKeys.json)
  configure_executable_companion_to_subdir("${subdir}/registrationserver" ${PEP_LOCAL_INFRA_DIR}/ShadowAdministration.pub)

  copy_pki_files(
      TARGET
          "${TARGET_PREFIX}_registrationserver_pki"
      SUBDIRECTORY
          "${subdir}/registrationserver"
      FILES
          rootCA.cert
          PEPRegistrationServer.key PEPRegistrationServer.chain
          TLSRegistrationServer.key TLSRegistrationServer.chain
  )

  configure_executable_companion_to_subdir("${subdir}/storagefacility" ${PEP_LOCAL_INFRA_DIR}/storagefacility/StorageFacility.json)
  configure_executable_companion_to_subdir("${subdir}/storagefacility" ${PEP_LOCAL_INFRA_DIR}/storagefacility/StorageFacilityKeys.json)

  copy_pki_files(
      TARGET
          "${TARGET_PREFIX}_storagefacility_pki"
      SUBDIRECTORY
          "${subdir}/storagefacility"
      FILES
          rootCA.cert
          PEPStorageFacility.key PEPStorageFacility.chain
          TLSStorageFacility.key TLSStorageFacility.chain
  )

  configure_executable_companion_to_subdir("${subdir}/transcryptor" ${PEP_LOCAL_INFRA_DIR}/transcryptor/Transcryptor.json)
  configure_executable_companion_to_subdir("${subdir}/transcryptor" ${PEP_LOCAL_INFRA_DIR}/transcryptor/SystemKeys.json)
  configure_executable_companion_to_subdir("${subdir}/transcryptor" ${PEP_LOCAL_INFRA_DIR}/transcryptor/Verifiers.json)
  configure_executable_companion_to_subdir("${subdir}/transcryptor" ${PEP_LOCAL_INFRA_DIR}/transcryptor/TranscryptorKeys.json)

  copy_pki_files(
      TARGET
          "${TARGET_PREFIX}_transcryptor_pki"
      SUBDIRECTORY
          "${subdir}/transcryptor"
      FILES
          rootCA.cert
          TLSTranscryptor.key TLSTranscryptor.chain
          PEPTranscryptor.key PEPTranscryptor.chain
  )

  configure_executable_companion_to_subdir("${subdir}/authserver" ${PEP_LOCAL_INFRA_DIR}/authserver/Authserver.json)
  configure_executable_companion_to_subdir("${subdir}/authserver" ${PEP_LOCAL_INFRA_DIR}/../OAuthTokenSecret.json OPTIONAL)
  configure_executable_companion_to_subdir("${subdir}/authserver" ${PEP_LOCAL_INFRA_DIR}/authserver/AuthserverHTTPSCertificate.pem OPTIONAL)
  configure_executable_companions_to_subdir_from("${subdir}/templates/authserver" "${CMAKE_SOURCE_DIR}/web/templates/authserver")
  configure_executable_companions_to_subdir_from("${subdir}/templates/common" "${CMAKE_SOURCE_DIR}/web/templates/common")

  copy_pki_files(
      TARGET
          "${TARGET_PREFIX}_authserver_pki"
      SUBDIRECTORY
          "${subdir}/authserver"
      FILES
          rootCA.cert
          PEPAuthserver.key PEPAuthserver.chain
          TLSAuthserver.key TLSAuthserver.chain
  )

  if(WITH_CASTOR AND DEFINED CASTOR_API_KEY_DIR)
    configure_executable_companion_to_subdir("${subdir}/registrationserver" "${CASTOR_API_KEY_DIR}/CastorAPIKey.json" ABSOLUTE)
  endif()

  # The local page store does not automatically create its directories:
  get_build_output_directories(directories "${subdir}/storagefacility/data/myBucket")
  foreach (dir ${directories})
    file(MAKE_DIRECTORY ${dir})
  endforeach()
endfunction()

add_library(${PROJECT_NAME}Serverslib STATIC
  Servers.hpp Servers.cpp
)
target_link_libraries(${PROJECT_NAME}Serverslib
    ${PROJECT_NAME}AccessManagerlib
    ${PROJECT_NAME}KeyServerlib
    ${PROJECT_NAME}RegistrationServerlib
    ${PROJECT_NAME}StorageFacilitylib
    ${PROJECT_NAME}Transcryptorlib
    ${PROJECT_NAME}Authserverlib
    ${PROJECT_NAME}ServiceApplicationlib
)
verify_headers_standalone(${PROJECT_NAME}Serverslib)


# Stage s3proxy so we can use it as a backend for Storage Facility's "s3" page store.
# We stage to the "storagefacility" subdirectory so we can switch (SF configuration)
# between "s3" and "local", since both will save their pages to the "data/myBucket"
# (sub)directory.
include(${PROJECT_SOURCE_DIR}/s3proxy/s3proxy.cmake)
stage_s3proxy("${PROJECT_NAME}Servers_s3proxy" "storagefacility")

add_cli_install_executable(${PROJECT_NAME}Servers Main.cpp)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}Servers PRIVATE VersionInfo.rc)
endif()
add_dependencies(
    ${PROJECT_NAME}Servers
    servers_accessmanager_pki
    servers_keyserver_pki
    servers_registrationserver_pki
    servers_storagefacility_pki
    servers_transcryptor_pki
    servers_authserver_pki
    ${PROJECT_NAME}Servers_s3proxy
    )
target_link_libraries(
    ${PROJECT_NAME}Servers
    ${PROJECT_NAME}Serverslib
    ${PROJECT_NAME}Applicationlib
    )

configure_debug_command_line_parameter(${PROJECT_NAME}Servers ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/)

configure_executable_config_version(${PROJECT_NAME}Servers local)

add_shared_companion_files(".")
