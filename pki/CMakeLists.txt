include(pki.cmake)

add_custom_target(
    pki ALL
    DEPENDS
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

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
# Make Windows command interpreter wait until Bash has finished running our script
	set(INVOKE_SHELLSCRIPT start /wait CACHE STRING "The command used to run shellscripts")
else()
	set(INVOKE_SHELLSCRIPT "")
endif()

add_custom_command(
	OUTPUT
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
	DEPENDS
		pki.sh
	DEPENDS
		ca_ext.cnf
	COMMAND
		${INVOKE_SHELLSCRIPT} ${CMAKE_CURRENT_SOURCE_DIR}/pki.sh
)
