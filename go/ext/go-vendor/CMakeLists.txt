include(ExternalProject)

ExternalProject_Add(go-vendor
    GIT_REPOSITORY "https://gitlab.pep.cs.ru.nl/mirror/go-vendor.git"
    GIT_TAG a1f89b6b51c68276f3c32e8a45e7ccbff195c78c
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/go/src
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND "")

set(GOPATH "${GOPATH}${PATH_DELIMITER}${CMAKE_CURRENT_BINARY_DIR}/go" PARENT_SCOPE)
