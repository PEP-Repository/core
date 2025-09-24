# (Provides a target that) generates configVersion.*.json files that
# specify the Gitlab version of the current build.
# Also provides a function that can stage these files to executable
# (debug) directories.

set(PEP_CONFIG_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})
# Determine (path to) files for local infrastructure
set(PEP_LOCAL_INFRA_DIR ${PEP_CONFIG_ROOT_DIR}/local)
# Ensure that path is normalized so it can be compared to other (normalized) paths
get_filename_component(PEP_LOCAL_INFRA_DIR ${PEP_LOCAL_INFRA_DIR} ABSOLUTE)

# List of files that on change need to trigger regeneration of the configVersion.*.json files 
SET(CONFIG_VERSION_DEPENDENCIES ${PROJECT_SOURCE_DIR}/scripts/createConfigVersionJson.sh ${PROJECT_SOURCE_DIR}/version.json)

# Which server infrastructure the client should connect to.
if(PEP_INFRA_DIR)
    # Interpret (possibly relative) infra dir spec in relation to where CMake was invoked
    get_filename_component(PEP_INFRA_DIR ${PEP_INFRA_DIR} ABSOLUTE BASE_DIR ${CMAKE_BINARY_DIR})
else()
    # Default to "local" infrastructure
    set(PEP_INFRA_DIR "${PEP_LOCAL_INFRA_DIR}")
endif()

if(NOT EXISTS "${PEP_INFRA_DIR}")
    message(FATAL_ERROR "Cannot find infrastructure directory at ${PEP_INFRA_DIR}")
endif()

if("${PEP_INFRA_DIR}" STREQUAL "${PEP_LOCAL_INFRA_DIR}")
    set(PEP_LOCAL_INFRA true)
else()
    set(PEP_LOCAL_INFRA false)
endif()

if(EXISTS "${PEP_INFRA_DIR}/project")
    set(PEP_INFRA_PROJECT_DIR "${PEP_INFRA_DIR}/project")
endif()

if (PEP_PROJECT_DIR)
    # Interpret (possibly relative) project dir spec in relation to where CMake was invoked
    get_filename_component(PEP_PROJECT_DIR ${PEP_PROJECT_DIR} ABSOLUTE BASE_DIR ${CMAKE_BINARY_DIR})
    if(PEP_INFRA_PROJECT_DIR AND NOT ("${PEP_INFRA_PROJECT_DIR}" STREQUAL "${PEP_PROJECT_DIR}"))
        message(FATAL_ERROR "Cannot specify project directory ${PEP_PROJECT_DIR} because infrastructure is coupled with project directory at ${PEP_INFRA_PROJECT_DIR}")
    endif()
elseif(PEP_INFRA_PROJECT_DIR)
    set(PEP_PROJECT_DIR "${PEP_INFRA_PROJECT_DIR}")
else()
    set(PEP_PROJECT_DIR "${PEP_CONFIG_ROOT_DIR}/projects/gum")
endif()

if(NOT EXISTS "${PEP_PROJECT_DIR}")
    message(FATAL_ERROR "Cannot find project directory at ${PEP_PROJECT_DIR}")
endif()

message("Using infra   config files from directory ${PEP_INFRA_DIR}")
message("Using project config files from directory ${PEP_PROJECT_DIR}")

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    execute_process(
        COMMAND cmd /c ${PROJECT_SOURCE_DIR}/scripts/windows-to-sh-path.bat ${PEP_INFRA_DIR}
        OUTPUT_VARIABLE PEP_SH_INFRA_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND cmd /c ${PROJECT_SOURCE_DIR}/scripts/windows-to-sh-path.bat ${PEP_LOCAL_INFRA_DIR}
        OUTPUT_VARIABLE PEP_SH_LOCAL_INFRA_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND cmd /c ${PROJECT_SOURCE_DIR}/scripts/windows-to-sh-path.bat ${PEP_PROJECT_DIR}
        OUTPUT_VARIABLE PEP_SH_PROJECT_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(PEP_SH_INFRA_DIR "${PEP_INFRA_DIR}")
    set(PEP_SH_LOCAL_INFRA_DIR "${PEP_LOCAL_INFRA_DIR}")
    set(PEP_SH_PROJECT_DIR "${PEP_PROJECT_DIR}")
endif()

add_custom_command(
  OUTPUT configVersion.infra.json
  COMMAND ${INVOKE_SH} ${PROJECT_SOURCE_DIR}/scripts/createConfigVersionJson.sh "${PEP_SH_INFRA_DIR}" "${PEP_SH_PROJECT_DIR}" > "configVersion.infra.json"
  DEPENDS ${CONFIG_VERSION_DEPENDENCIES}
  # TODO outdate when PEP_INFRA_DIR changes
)
add_custom_command(
  OUTPUT configVersion.local.json
  COMMAND ${INVOKE_SH} ${PROJECT_SOURCE_DIR}/scripts/createConfigVersionJson.sh "${PEP_SH_LOCAL_INFRA_DIR}" "${PEP_SH_PROJECT_DIR}" > "configVersion.local.json"
  DEPENDS ${CONFIG_VERSION_DEPENDENCIES}
)
add_custom_target (${PROJECT_NAME}ConfigVersions DEPENDS configVersion.infra.json configVersion.local.json)

function(configure_executable_config_version target infra)
  get_build_output_directories(destination_dirs .)
  FOREACH(destination ${destination_dirs})
    add_custom_command(
      OUTPUT ${destination}/configVersion.json
      COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/configVersion.${infra}.json ${destination}/configVersion.json
      DEPENDS ${PROJECT_NAME}ConfigVersions ${PROJECT_BINARY_DIR}/configVersion.${infra}.json
    )
    list(APPEND destination_files "${destination}/configVersion.json")
  ENDFOREACH()
  add_custom_target(
    ${target}_ConfigVersion ALL
    DEPENDS ${destination_files}
  )
  add_dependencies(${target} ${target}_ConfigVersion)
endfunction()
