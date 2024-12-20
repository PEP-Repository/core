# Functionality related to auth[entication|orization].

add_library(${PROJECT_NAME}Authlib STATIC
    FacilityType.cpp FacilityType.hpp
    OAuthToken.cpp OAuthToken.hpp
    UserGroups.cpp UserGroups.hpp
)

target_link_libraries(${PROJECT_NAME}Authlib
  ${PROJECT_NAME}Cryptolib
)


add_unit_tests(Auth)
