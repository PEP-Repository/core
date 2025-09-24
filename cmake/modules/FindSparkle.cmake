#
# Find the Sparkle framework
#
# Adapted from:
# https://gitlab.com/wireshark/wireshark/-/blob/master/cmake/modules/FindSparkle.cmake

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

  include(FindPackageHandleStandardArgs)

  file(GLOB USR_LOCAL_HINT "/usr/local/Sparkle-[2-9]*/")
  file(GLOB HOMEBREW_ARM64_HINT "/opt/homebrew/Caskroom/sparkle/[2-9]*/")
  file(GLOB HOMEBREW_x86_HINT "/usr/local/Caskroom/sparkle/[2-9]*/")

  find_path(SPARKLE_INCLUDE_DIR Sparkle.h
    HINTS ${USR_LOCAL_HINT} ${HOMEBREW_x86_HINT} ${HOMEBREW_ARM64_HINT}
  )

  find_library(SPARKLE_LIBRARY NAMES Sparkle
    HINTS ${USR_LOCAL_HINT} ${HOMEBREW_x86_HINT} ${HOMEBREW_ARM64_HINT}
  )

  find_package_handle_standard_args(Sparkle
    REQUIRED_VARS SPARKLE_INCLUDE_DIR SPARKLE_LIBRARY
  )

  if (SPARKLE_FOUND)
    add_library(Sparkle::Sparkle INTERFACE IMPORTED)

    set_property(TARGET Sparkle::Sparkle PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES ${SPARKLE_INCLUDE_DIR}
    )
    set_property(TARGET Sparkle::Sparkle PROPERTY
      INTERFACE_LINK_LIBRARIES ${SPARKLE_LIBRARY}
    )
    set_property(TARGET Sparkle::Sparkle PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
  endif ()

  mark_as_advanced(SPARKLE_INCLUDE_DIR SPARKLE_LIBRARY)

endif()