# Library providing archiving functionality based on libarchive.

add_library(${PROJECT_NAME}Archivinglib STATIC
    Archive.cpp Archive.hpp
    DirectoryArchive.cpp DirectoryArchive.hpp
    HashedArchive.cpp HashedArchive.hpp
    NOPArchive.hpp
    PseudonymiseInputFilter.hpp
    Pseudonymiser.cpp Pseudonymiser.hpp
    Tar.cpp Tar.hpp
)

find_package(Boost REQUIRED COMPONENTS
  iostreams
)
find_package(LibArchive REQUIRED)

target_link_libraries(${PROJECT_NAME}Archivinglib
  ${PROJECT_NAME}Utilslib
  Boost::iostreams
  LibArchive::LibArchive
)

add_unit_tests(Archiving)
