add_custom_target(${PROJECT_NAME}watchdog  ALL
  COMMAND ${CMAKE_COMMAND} -E env "GOPATH=${GOPATH}" "GO111MODULE=off" go build ${GO_BUILD_FLAGS} -v pep.cs.ru.nl/pep-watchdog
  DEPENDS ${PROJECT_NAME}gopep-messages 
# find . | grep ".*.go" | cut -c 3- 
selfCheck.go
mail/main.go
shared/conf.go
shared/args.go
shared/main.go
shared/tokenFile.go
state/persistentIssues.go
state/condition.go
state/report.go
state/main.go
http/badge.go
http/metricsProxy.go
http/status.go
http/main.go
http/root.go
stressing.go
check/httpsCertificates.go
check/misc.go
check/pepVersions.go
check/core.go
check/checksumChains.go
check/watchdogWatchdog.go
check/pipelines.go
check/pepCertificates.go
check/canaries.go
consts/consts.go
main.go
export.go
canaries/canaries.go
  )


configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/watchdog/config.yaml)
configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/watchdog/constellation.yaml)
configure_executable_companion(${PEP_LOCAL_INFRA_DIR}/watchdog/token.yaml)

configure_executable_config_version(${PROJECT_NAME}watchdog local)

include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
copy_pki_files(TARGET watchdog_pki FILES rootCA.cert)
add_dependencies(${PROJECT_NAME}watchdog watchdog_pki)
