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

find_package(Boost REQUIRED)
find_package(LibArchive REQUIRED)
target_link_libraries(${PROJECT_NAME}Archivinglib
PUBLIC
  ${PROJECT_NAME}Utilslib
  Boost::iostreams
PRIVATE
  LibArchive::LibArchive
)

verify_headers_standalone(${PROJECT_NAME}Archivinglib)
add_unit_tests(Archiving)
