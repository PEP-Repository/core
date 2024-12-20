add_custom_target(${PROJECT_NAME}checkchains  ALL
  COMMAND ${CMAKE_COMMAND} -E env "GOPATH=${GOPATH}" "GO111MODULE=off" go build ${GO_BUILD_FLAGS} -v pep.cs.ru.nl/pep-checkchains
  DEPENDS ${PROJECT_NAME}gopep-messages main.go)


