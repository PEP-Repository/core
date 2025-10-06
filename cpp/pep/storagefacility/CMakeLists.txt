add_library(${PROJECT_NAME}StorageFacilityApilib
  Constants.hpp
  DataPayloadPage.cpp DataPayloadPage.hpp
  PageHash.cpp PageHash.hpp
  StorageFacilityMessages.hpp
  StorageFacilitySerializers.cpp StorageFacilitySerializers.hpp
)
target_link_libraries(${PROJECT_NAME}StorageFacilityApilib
  ${PROJECT_NAME}Morphinglib
  ${PROJECT_NAME}Ticketinglib
)
verify_headers_standalone(${PROJECT_NAME}StorageFacilityApilib)

add_library(${PROJECT_NAME}StorageClientlib
  StorageClient.cpp StorageClient.hpp
)
target_link_libraries(${PROJECT_NAME}StorageClientlib
  ${PROJECT_NAME}StorageFacilityApilib
  ${PROJECT_NAME}TypedClientlib
)
verify_headers_standalone(${PROJECT_NAME}StorageClientlib)


if (WITH_SERVERS)
  add_library(${PROJECT_NAME}StorageFacilitylib
      EntryContent.cpp EntryContent.hpp
      EntryName.cpp EntryName.hpp
      EntryPayload.cpp EntryPayload.hpp
      PersistedEntryProperties.cpp PersistedEntryProperties.hpp
      FileStore.cpp FileStore.hpp
      PageStore.cpp PageStore.hpp
      S3.cpp S3.hpp
      S3Client.cpp S3Client.hpp
      S3Credentials.hpp
      S3Credentials.PropertySerializer.cpp S3Credentials.PropertySerializer.hpp
      SFId.hpp
      SFIdSerializer.cpp SFIdSerializer.hpp
      StorageFacility.cpp StorageFacility.hpp
  )
  target_link_libraries(${PROJECT_NAME}StorageFacilitylib
    ${PROJECT_NAME}StorageFacilityApilib
    ${PROJECT_NAME}Serverlib
  )
  verify_headers_standalone(${PROJECT_NAME}StorageFacilitylib)

  # Stage s3proxy so we can use it as a backend for Storage Facility's "s3" page store.
  include(${PROJECT_SOURCE_DIR}/s3proxy/s3proxy.cmake)
  # TODO: stage to a subdir so that we can switch between "s3" and "local" page store configuration.
  #       See CMakeLists.txt in the "servers" directory.
  stage_s3proxy("${PROJECT_NAME}StorageFacility_s3proxy" "")

  add_cli_install_executable(${PROJECT_NAME}StorageFacility Main.cpp)
  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME}StorageFacility PRIVATE VersionInfo.rc)
  endif()

  target_link_libraries(${PROJECT_NAME}StorageFacility
    ${PROJECT_NAME}StorageFacilitylib
    ${PROJECT_NAME}ServiceApplicationlib
  )
  add_dependencies(${PROJECT_NAME}StorageFacility ${PROJECT_NAME}StorageFacility_s3proxy) # Make s3proxy available as a page store backend

  configure_service_debugging(StorageFacility)

  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/storagefacility/StorageFacility.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/storagefacility/StorageFacilityKeys.json)
  configure_executable_config_version(${PROJECT_NAME}StorageFacility local)

  # The local page store does not automatically create its directories:
  get_build_output_directories(directories "data/myBucket")
  foreach (dir ${directories})
    file(MAKE_DIRECTORY ${dir})
  endforeach()

  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(
      TARGET
          storagefacility_pki
      FILES
          rootCA.cert
          PEPStorageFacility.key PEPStorageFacility.chain
          TLSStorageFacility.key TLSStorageFacility.chain
  )

  if (WITH_TESTS)
    add_unit_tests(StorageFacility EXTRA_ARGS "--launch-s3proxy")
    add_dependencies(${PROJECT_NAME}StorageFacilityUnitTests ${PROJECT_NAME}StorageFacility_s3proxy storagefacility_pki)
  endif ()
endif()
