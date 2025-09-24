# Taken from https://github.com/google/glog/blob/224f66bb63393b63d254cac3ff67c2b247a95a36/cmake/FindUnwind.cmake
# And modified unwind->libunwind

# - Try to find libunwind
# Once done this will define
#
#  Libunwind_FOUND - system has libunwind
#  libunwind::libunwind - cmake target for libunwind

include (FindPackageHandleStandardArgs)

find_path (Libunwind_INCLUDE_DIR NAMES libunwind.h libunwind.h DOC "libunwind include directory")
find_library (Libunwind_LIBRARY NAMES unwind DOC "libunwind library")

mark_as_advanced (Libunwind_INCLUDE_DIR Libunwind_LIBRARY)

# Extract version information
if (Libunwind_LIBRARY)
  set (_Libunwind_VERSION_HEADER ${Libunwind_INCLUDE_DIR}/libunwind-common.h)

  if (EXISTS ${_Libunwind_VERSION_HEADER})
    file (READ ${_Libunwind_VERSION_HEADER} _Libunwind_VERSION_CONTENTS)

    string (REGEX REPLACE ".*#define UNW_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1"
      Libunwind_VERSION_MAJOR "${_Libunwind_VERSION_CONTENTS}")
    string (REGEX REPLACE ".*#define UNW_VERSION_MINOR[ \t]+([0-9]+).*" "\\1"
      Libunwind_VERSION_MINOR "${_Libunwind_VERSION_CONTENTS}")
    string (REGEX REPLACE ".*#define UNW_VERSION_EXTRA[ \t]+([0-9]+).*" "\\1"
      Libunwind_VERSION_PATCH "${_Libunwind_VERSION_CONTENTS}")

    set (Libunwind_VERSION ${Libunwind_VERSION_MAJOR}.${Libunwind_VERSION_MINOR})

    if (CMAKE_MATCH_0)
      # Third version component may be empty
      set (Libunwind_VERSION ${Libunwind_VERSION}.${Libunwind_VERSION_PATCH})
      set (Libunwind_VERSION_COMPONENTS 3)
    else (CMAKE_MATCH_0)
      set (Libunwind_VERSION_COMPONENTS 2)
    endif (CMAKE_MATCH_0)
  endif (EXISTS ${_Libunwind_VERSION_HEADER})
endif (Libunwind_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set Libunwind_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args (Libunwind
  REQUIRED_VARS Libunwind_INCLUDE_DIR Libunwind_LIBRARY
  VERSION_VAR Libunwind_VERSION
)

if (Libunwind_FOUND)
  if (NOT TARGET libunwind::libunwind)
    add_library (libunwind::libunwind INTERFACE IMPORTED)

    set_property (TARGET libunwind::libunwind PROPERTY
      INTERFACE_INCLUDE_DIRECTORIES ${Libunwind_INCLUDE_DIR}
    )
    set_property (TARGET libunwind::libunwind PROPERTY
      INTERFACE_LINK_LIBRARIES ${Libunwind_LIBRARY}
    )
    set_property (TARGET libunwind::libunwind PROPERTY
      IMPORTED_CONFIGURATIONS RELEASE
    )
  endif (NOT TARGET libunwind::libunwind)
endif (Libunwind_FOUND)
