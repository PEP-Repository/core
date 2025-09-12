add_library(${PROJECT_NAME}TranscryptorApilib
  KeyComponentMessages.cpp KeyComponentMessages.hpp
  KeyComponentSerializers.cpp KeyComponentSerializers.hpp
  TranscryptorMessages.cpp TranscryptorMessages.hpp
  TranscryptorSerializers.cpp TranscryptorSerializers.hpp
)

target_link_libraries(${PROJECT_NAME}TranscryptorApilib
  ${PROJECT_NAME}Morphinglib
  ${PROJECT_NAME}Ticketinglib
)
verify_headers_standalone(${PROJECT_NAME}TranscryptorApilib)

if (WITH_SERVERS)
  add_library(${PROJECT_NAME}Transcryptorlib
      Storage.cpp Storage.hpp
      Transcryptor.cpp Transcryptor.hpp
      ChecksumChain.cpp ChecksumChain.hpp
  )

  find_package(SqliteOrm REQUIRED)
  target_link_libraries(${PROJECT_NAME}Transcryptorlib
    ${PROJECT_NAME}TranscryptorApilib
    ${PROJECT_NAME}Serverlib
    ${PROJECT_NAME}Databaselib
  )

  verify_headers_standalone(${PROJECT_NAME}Transcryptorlib)

  add_cli_install_executable(${PROJECT_NAME}Transcryptor Main.cpp)
  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME}Transcryptor PRIVATE VersionInfo.rc)
  endif()
  target_link_libraries(${PROJECT_NAME}Transcryptor
    ${PROJECT_NAME}Transcryptorlib
    ${PROJECT_NAME}ServiceApplicationlib
  )

  configure_service_debugging(Transcryptor)

  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/transcryptor/Transcryptor.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/transcryptor/SystemKeys.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/transcryptor/Verifiers.json)
  configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/transcryptor/TranscryptorKeys.json)
  configure_executable_config_version(${PROJECT_NAME}Transcryptor local)

  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(
      TARGET
          transcryptor_pki
      FILES
          rootCA.cert
          TLSTranscryptor.key TLSTranscryptor.chain
          PEPTranscryptor.key PEPTranscryptor.chain
  )
endif()
