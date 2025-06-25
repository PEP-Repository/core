# Functionality for SQLite databases.

# Remove INTERFACE here & below  if you add any .cpp files
add_library(${PROJECT_NAME}Databaselib INTERFACE
  SqliteOrmUtils.hpp
)
find_package(SqliteOrm REQUIRED)
target_link_libraries(${PROJECT_NAME}Databaselib INTERFACE
  ${PROJECT_NAME}Utilslib
  sqlite_orm::sqlite_orm
)
verify_headers_standalone(${PROJECT_NAME}Databaselib)
