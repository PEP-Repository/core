add_custom_target(${PROJECT_NAME}watchdog-watchdog  ALL
  COMMAND ${CMAKE_COMMAND} -E env "GOPATH=${GOPATH}" "GO111MODULE=off" go build ${GO_BUILD_FLAGS} -v pep.cs.ru.nl/pep-watchdog-watchdog
  DEPENDS go-vendor main.go)
