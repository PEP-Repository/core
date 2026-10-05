include(pki.cmake)

set(PKI_DIR "" CACHE STRING "Directory to copy PKI from, or empty to generate")

set(pki_files
    PEPAccessManager.chain
    PEPAccessManager.key
    PEPAuthserver.chain
    PEPAuthserver.key
    PEPRegistrationServer.chain
    PEPRegistrationServer.key
    PEPStorageFacility.chain
    PEPStorageFacility.key
    PEPTranscryptor.chain
    PEPTranscryptor.key
    TLSAccessManager.chain
    TLSAccessManager.key
    TLSAuthserver.chain
    TLSAuthserver.key
    TLSKeyServer.chain
    TLSKeyServer.key
    TLSRegistrationServer.chain
    TLSRegistrationServer.key
    TLSStorageFacility.chain
    TLSStorageFacility.key
    TLSTranscryptor.chain
    TLSTranscryptor.key
    pepClientCA.chain
    pepClientCA.key
    rootCA.cert
    s3certs/private.key
    s3certs/public.crt
)

if(PKI_DIR)
  get_filename_component(PKI_DIR ${PKI_DIR} ABSOLUTE BASE_DIR ${CMAKE_SOURCE_DIR})
  add_custom_command(
      OUTPUT ${pki_files}
      DEPENDS ${PKI_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${pki_files} ${CMAKE_CURRENT_BINARY_DIR}
      WORKING_DIRECTORY ${PKI_DIR}
  )
else()
  add_custom_command(
      OUTPUT ${pki_files}
      DEPENDS pki.sh ca_ext.cnf
      COMMAND ${INVOKE_SH} ${CMAKE_CURRENT_SOURCE_DIR}/pki.sh
  )
endif()

add_custom_target(
    pki ALL
    DEPENDS ${pki_files}
)
