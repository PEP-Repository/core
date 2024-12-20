# Inspired by: https://github.com/curl/curl/blob/2bc1d775f510196154283374284f98d3eae03544/CMake/FindMbedTLS.cmake

include(FindPackageHandleStandardArgs)

find_path(MbedTLS_INCLUDE_DIR NAMES mbedtls/ssl.h)

find_library(MbedTLS_LIBRARY mbedtls)
find_library(MbedX509_LIBRARY mbedx509)
find_library(MbedCrypto_LIBRARY mbedcrypto)
mark_as_advanced(MbedTLS_INCLUDE_DIR MbedTLS_LIBRARY MbedX509_LIBRARY MbedCrypto_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set MbedTLS_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(MbedTLS
  REQUIRED_VARS MbedTLS_INCLUDE_DIR MbedTLS_LIBRARY MbedX509_LIBRARY MbedCrypto_LIBRARY
  VERSION_VAR MbedTLS_VERSION
  HANDLE_VERSION_RANGE
)

if (MbedTLS_FOUND)
  if (NOT TARGET MbedTLS::mbedtls)
    add_library(MbedTLS::mbedtls INTERFACE IMPORTED)

    set_property(TARGET MbedTLS::mbedtls PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES ${MbedTLS_INCLUDE_DIR}
    )
    set_property(TARGET MbedTLS::mbedtls PROPERTY
      INTERFACE_LINK_LIBRARIES ${MbedTLS_LIBRARY}
    )
    set_property(TARGET MbedTLS::mbedtls PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
  endif ()
    if (NOT TARGET MbedTLS::mbedx509)
    add_library(MbedTLS::mbedx509 INTERFACE IMPORTED)

    set_property(TARGET MbedTLS::mbedx509 PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES ${MbedTLS_INCLUDE_DIR}
    )
    set_property(TARGET MbedTLS::mbedx509 PROPERTY
      INTERFACE_LINK_LIBRARIES ${MbedX509_LIBRARY}
    )
    set_property(TARGET MbedTLS::mbedx509 PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
  endif ()
    if (NOT TARGET MbedTLS::mbedcrypto)
    add_library(MbedTLS::mbedcrypto INTERFACE IMPORTED)

    set_property(TARGET MbedTLS::mbedcrypto PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES ${MbedTLS_INCLUDE_DIR}
    )
    set_property(TARGET MbedTLS::mbedcrypto PROPERTY
      INTERFACE_LINK_LIBRARIES ${MbedCrypto_LIBRARY}
    )
    set_property(TARGET MbedTLS::mbedcrypto PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
  endif ()
endif ()
