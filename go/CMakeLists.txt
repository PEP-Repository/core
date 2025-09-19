cmake_minimum_required(VERSION 3.20)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(PATH_DELIMITER ";")
else()
    set(PATH_DELIMITER ":")
endif()

set(GO_EXTRA_BUILD_FLAGS "" CACHE STRING "Extra go build flags such as -race or -msan")
set(PEP_GO_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if("$ENV{GOPATH}" STREQUAL "")
set(GOPATH ${PEP_GO_ROOT_PATH})
else()
set(GOPATH $ENV{GOPATH}${PATH_DELIMITER}${PEP_GO_ROOT_PATH})
endif()

set(PEP_GOPATH ${CMAKE_CURRENT_BINARY_DIR})
set(GOPATH ${GOPATH}${PATH_DELIMITER}${PEP_GOPATH})

if(NOT DEFINED MAJOR_VERSION)
  message(FATAL_ERROR "Major version is not set.")
endif()
if(NOT DEFINED MINOR_VERSION)
  message(FATAL_ERROR "Minor version is not set.")
endif()

# If you update the build variables, don't forget to update those
# for C(++) as well.
set(GO_BUILD_FLAGS "${GO_EXTRA_BUILD_FLAGS}" -ldflags
       "-X pep.cs.ru.nl/gopep/build.BuildProjectPath=$ENV{CI_PROJECT_PATH} \
        -X pep.cs.ru.nl/gopep/build.BuildReference=$ENV{CI_COMMIT_REF_NAME} \
        -X pep.cs.ru.nl/gopep/build.BuildRevision=$ENV{CI_COMMIT_SHA} \
        -X pep.cs.ru.nl/gopep/build.BuildMajorVersion=${MAJOR_VERSION} \
        -X pep.cs.ru.nl/gopep/build.BuildMinorVersion=${MINOR_VERSION} \
        -X pep.cs.ru.nl/gopep/build.BuildPipelineId=${CI_PIPELINE_ID} \
        -X pep.cs.ru.nl/gopep/build.BuildJobId=${CI_JOB_ID} \
        -X pep.cs.ru.nl/gopep/build.BuildTarget=${CMAKE_SYSTEM_NAME}")

# Include external stuff
add_subdirectory(${PEP_GO_ROOT_PATH}/ext/go-vendor)

# Include our own project(directorie)s
set(PEP_GO_SOURCE_PATH ${PEP_GO_ROOT_PATH}/src/pep.cs.ru.nl)
add_subdirectory(${PEP_GO_SOURCE_PATH}/gopep)
add_subdirectory(${PEP_GO_SOURCE_PATH}/pep-watchdog)
add_subdirectory(${PEP_GO_SOURCE_PATH}/pep-checkchains)
