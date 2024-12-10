# Library related to the structure of PEP's storage.


add_library(${PROJECT_NAME}Structurelib STATIC
  ColumnName.cpp ColumnName.hpp
  ColumnNameSerializers.cpp ColumnNameSerializers.hpp
  GlobalConfiguration.cpp GlobalConfiguration.hpp
  StructureSerializers.cpp StructureSerializers.hpp
  ShortPseudonyms.cpp ShortPseudonyms.hpp
  StudyContext.cpp StudyContext.hpp
)

target_link_libraries(${PROJECT_NAME}Structurelib
  ${PROJECT_NAME}RskPeplib
)

add_unit_tests(Structure)
