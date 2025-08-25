# Library related to the structure of PEP's storage.


add_library(${PROJECT_NAME}Structurelib STATIC
  ColumnName.cpp ColumnName.hpp
  ColumnNameSerializers.cpp ColumnNameSerializers.hpp
  GlobalConfiguration.cpp GlobalConfiguration.hpp
  StructureSerializers.cpp StructureSerializers.hpp
  ShortPseudonyms.cpp ShortPseudonyms.hpp
  StudyContext.cpp StudyContext.hpp
)

find_package(Boost REQUIRED)
target_link_libraries(${PROJECT_NAME}Structurelib
PUBLIC
  ${PROJECT_NAME}RskPeplib
PRIVATE
  Boost::random
)

add_unit_tests(Structure)
verify_headers_standalone(${PROJECT_NAME}Structurelib)
