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

option(WITH_UNWINDER "Use unwinder" ON)
if (WITH_UNWINDER)
  add_definitions(-DWITH_UNWINDER)
  if (NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    find_package(Libunwind REQUIRED)
    target_link_libraries(${PROJECT_NAME}Applicationlib libunwind::libunwind)
  endif ()
endif ()

target_link_libraries(${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Versioninglib
)
