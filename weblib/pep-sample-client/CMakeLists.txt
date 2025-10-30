# NPM wants to be ran in the source folder, so we need to link/copy stuff from the build folder to there

add_custom_target(pep-sample-client-npm
    COMMENT "Install dependencies"
    COMMAND ${NPM_PROGRAM} install
    VERBATIM
    DEPENDS ${PROJECT_NAME}WeblibJs
    BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/node_modules"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_target(pep-sample-client-build
    COMMENT "Type-check code and bundle dependencies"
    COMMAND ${NPM_PROGRAM} run types:check
    COMMAND ${NPM_PROGRAM} run "$<IF:$<CONFIG:Debug>,build-dev,build>"
    VERBATIM
    DEPENDS pep-sample-client-npm
    BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/dist"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set(separate_dwarf_copy_args_debug
    create_symlink
    $<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.wasm.debug.wasm
    ${CMAKE_CURRENT_SOURCE_DIR}/dist/${PROJECT_NAME}Weblib.wasm.debug.wasm
)
add_custom_target(pep-sample-client-separate-dwarf
    COMMENT "Copy -gseparate-dwarf debug info"
    # Use cat to skip trying to skip in Release mode
    COMMAND ${CMAKE_COMMAND} -E "$<IF:$<CONFIG:Debug,RelWithDebInfo>,${separate_dwarf_copy_args_debug},cat;-->"
    COMMAND_EXPAND_LISTS
    VERBATIM
    DEPENDS ${PROJECT_NAME}Weblib
)
add_custom_target(pep-sample-client-source-map
    COMMENT "Copy -gsource-map"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      $<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.wasm.map
      ${CMAKE_CURRENT_SOURCE_DIR}/dist/${PROJECT_NAME}Weblib.wasm.map
    COMMAND_EXPAND_LISTS
    VERBATIM
    DEPENDS ${PROJECT_NAME}Weblib
)

add_custom_target(pep-sample-client-config
    COMMENT "Copy config to dist folder"
    COMMAND ${CMAKE_COMMAND} -E copy
      ClientConfig.json
      rootCA.cert
      ShadowAdministration.pub
      "${CMAKE_CURRENT_SOURCE_DIR}/dist/"
    VERBATIM
    DEPENDS
      ClientConfig.json
      rootCA.cert
      ShadowAdministration.pub
    BYPRODUCTS
      "${CMAKE_CURRENT_SOURCE_DIR}/dist/ClientConfig.json"
      "${CMAKE_CURRENT_SOURCE_DIR}/dist/rootCA.cert"
      "${CMAKE_CURRENT_SOURCE_DIR}/dist/ShadowAdministration.pub"
)

add_custom_target(${PROJECT_NAME}WeblibSampleClient ALL
    COMMENT "Build full weblib sample client"
    DEPENDS
      pep-sample-client-build
      pep-sample-client-separate-dwarf
      pep-sample-client-source-map
      pep-sample-client-config
)

configure_executable_companion(${PEP_INFRA_DIR}/client/ClientConfig.json REPLACE_VARIABLES)
configure_executable_companion(${PEP_INFRA_DIR}/rootCA.cert OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/ShadowAdministration.pub OPTIONAL)
if(NOT EXISTS "${PEP_INFRA_DIR}/rootCA.cert")
  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(TARGET weblib_sample_client_pki FILES rootCA.cert)
  add_dependencies(${PROJECT_NAME}WeblibSampleClient weblib_sample_client_pki)
endif()
