# Library providing client connectivity to the core PEP servers:
# Access Manager, Transcryptor, and Storage Facility.
# For (client connectivity to) other servers, see the "pepClientlib".

add_library(${PROJECT_NAME}CoreClientlib STATIC
    CoreClient.AesKeys.cpp
    CoreClient.Ama.cpp
    CoreClient.Data.EnumAndRetrieve.cpp
    CoreClient.Data.Read.cpp
    CoreClient.Data.Write.cpp
    CoreClient.Enroll.cpp
    CoreClient.Users.cpp
    CoreClient.cpp CoreClient.hpp CoreClient_fwd.hpp
    CoreClient.ServerConnection.cpp CoreClient.ServerConnection.hpp
)

target_link_libraries(${PROJECT_NAME}CoreClientlib
  ${PROJECT_NAME}AccessManagerApilib
  ${PROJECT_NAME}StorageFacilityApilib
  ${PROJECT_NAME}ServerApilib
  ${PROJECT_NAME}Authlib
  ${PROJECT_NAME}Networkinglib
)

verify_headers_standalone(${PROJECT_NAME}CoreClientlib)
