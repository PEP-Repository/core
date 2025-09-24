# Library to produce structured and machine parseable cli-output
find_package(nlohmann_json REQUIRED)

add_library(${PROJECT_NAME}StructuredOutputlib STATIC
  Common.hpp
  Csv.hpp Csv.cpp
  FormatFlags.hpp FormatFlags.cpp
  IndexedStringPool.hpp
  Json.hpp Json.cpp
  Table.cpp Table.hpp
  Tree.hpp Tree.cpp
  Yaml.cpp Yaml.hpp
)

target_link_libraries(${PROJECT_NAME}StructuredOutputlib
  ${PROJECT_NAME}AuthserverApilib
  nlohmann_json::nlohmann_json
)

add_unit_tests(StructuredOutput)
verify_headers_standalone(${PROJECT_NAME}StructuredOutputlib)
