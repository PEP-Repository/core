# Functionality for applications. Only executable targets should need to link this library.

add_library(${PROJECT_NAME}Applicationlib STATIC
    Application.cpp Application.hpp
    CommandLineAutocomplete.hpp
    CommandLineCommand.cpp CommandLineCommand.hpp
    CommandLineHelp.cpp CommandLineHelp.hpp
    CommandLineParameter.cpp CommandLineParameter.hpp
    CommandLineSwitchAnnouncement.cpp CommandLineSwitchAnnouncement.hpp
    CommandLineUtility.cpp CommandLineUtility.hpp
    CommandLineValue.cpp CommandLineValue.hpp
    CommandLineValueParser.hpp
    CommandLineValueSpecification.hpp
    Unwinder.cpp Unwinder.hpp
)

target_link_libraries(${PROJECT_NAME}Applicationlib
  PUBLIC
    ${PROJECT_NAME}Versioninglib
)

option(WITH_UNWINDER "Use unwinder" ON)
if (WITH_UNWINDER)
  add_definitions(-DWITH_UNWINDER)
  if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    find_package(Libunwind REQUIRED)
    target_link_libraries(${PROJECT_NAME}Applicationlib
      PRIVATE
        libunwind::libunwind
    )
  endif ()
endif ()

verify_headers_standalone(${PROJECT_NAME}Applicationlib)
