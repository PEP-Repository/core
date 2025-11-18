# Functionality related to auth[entication|orization].

add_library(${PROJECT_NAME}Authlib STATIC
    EnrolledParty.cpp EnrolledParty.hpp
    OAuthError.cpp OAuthError.hpp
    OAuthToken.cpp OAuthToken.hpp
    UserGroup.cpp UserGroup.hpp
    UserGroupSerializers.cpp UserGroupSerializers.hpp
)

find_package(Boost REQUIRED)

target_link_libraries(${PROJECT_NAME}Authlib
  ${PROJECT_NAME}Cryptolib
  Boost::url
)


verify_headers_standalone(${PROJECT_NAME}Authlib)
add_unit_tests(Auth)
if(TARGET ${PROJECT_NAME}AuthUnitTests)
  target_sources(${PROJECT_NAME}AuthUnitTests PRIVATE
    ../crypto/tests/X509Certificate.Samples.test.cpp
  )
endif()
