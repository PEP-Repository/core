# Library providing OAuth client functionality.

set(sources
    OAuthClient.cpp OAuthClient.hpp
    ConsoleAuthorization.cpp ConsoleAuthorization.hpp
)
if(NOT EMSCRIPTEN)
  set(oauth_browser_auth ON)
else()
  set(oauth_browser_auth OFF)
endif()

if(oauth_browser_auth)
  list(APPEND sources
      BrowserAuthorization.cpp BrowserAuthorization.hpp
  )
endif()

add_library(${PROJECT_NAME}OAuthClientlib ${sources})
target_link_libraries(${PROJECT_NAME}OAuthClientlib
  PUBLIC
    ${PROJECT_NAME}Authlib
    ${PROJECT_NAME}Networkinglib
)

if(oauth_browser_auth)
  # See conditional source files above
  target_link_libraries(${PROJECT_NAME}OAuthClientlib
    PRIVATE
      ${PROJECT_NAME}Httpserverlib
  )
  if(LINUX OR CMAKE_SYSTEM_NAME STREQUAL Darwin)
    find_package(Boost REQUIRED)
    target_link_libraries(${PROJECT_NAME}OAuthClientlib
      PRIVATE
        Boost::process
    )
  endif()
endif()

verify_headers_standalone(${PROJECT_NAME}OAuthClientlib)
