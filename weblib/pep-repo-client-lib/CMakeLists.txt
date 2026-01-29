# NPM wants to be run in the source folder, so we need to link/copy stuff from the build folder to there

# Note: We're using add_custom_target instead of add_custom_command because we always want this to run,
# especially when some other configuration might've messed with the folders

add_custom_target(pep-repo-client-wasm-symlink
    COMMENT "Link pep-repo-client-wasm in source tree to our weblib build folder"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
      "$<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>"
      "${CMAKE_CURRENT_SOURCE_DIR}/../pep-repo-client-wasm"
    VERBATIM
    DEPENDS ${PROJECT_NAME}Weblib
    BYPRODUCTS "${CMAKE_CURRENT_SOURCE_DIR}/../pep-repo-client-wasm"
)

add_custom_target(pep-repo-client-lib-npm
    COMMENT "Install dependencies for pep-repo-client-lib"
    COMMAND ${NPM_PROGRAM} install
    VERBATIM
    DEPENDS pep-repo-client-wasm-symlink
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_target_properties(pep-repo-client-lib-npm PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/"
)

add_custom_target(pep-repo-client-lib-build
    COMMENT "Type-check & compile to JS for pep-repo-client-lib"
    COMMAND ${NPM_PROGRAM} run build
    VERBATIM
    DEPENDS pep-repo-client-lib-npm
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_target_properties(pep-repo-client-lib-build PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/dist/"
)

add_custom_target(pep-repo-client-lib-config
    COMMENT "Copy config to dist-test folder"
    COMMAND ${CMAKE_COMMAND} -E copy
      ClientConfig.json
      rootCA.cert
      ShadowAdministration.pub
      OAuthTokenSecret.json
      "${CMAKE_CURRENT_SOURCE_DIR}/dist-test/"
    VERBATIM
    DEPENDS
      ClientConfig.json
      rootCA.cert
      ShadowAdministration.pub
      OAuthTokenSecret.json
    BYPRODUCTS
      "${CMAKE_CURRENT_SOURCE_DIR}/dist-test/ClientConfig.json"
      "${CMAKE_CURRENT_SOURCE_DIR}/dist-test/rootCA.cert"
      "${CMAKE_CURRENT_SOURCE_DIR}/dist-test/ShadowAdministration.pub"
      "${CMAKE_CURRENT_SOURCE_DIR}/dist-test/OAuthTokenSecret.json"
)

add_custom_target(${PROJECT_NAME}WeblibJs ALL
    COMMENT "Build full JS weblib"
    DEPENDS
      pep-repo-client-lib-build
      pep-repo-client-lib-config
)


set(separate_dwarf_copy_args_debug
    create_symlink
    $<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.wasm.debug.wasm
    ${CMAKE_CURRENT_SOURCE_DIR}/dist-test/${PROJECT_NAME}Weblib.wasm.debug.wasm
)
add_custom_target(pep-repo-client-lib-separate-dwarf
    COMMENT "Copy -gseparate-dwarf debug info for pep-repo-client-lib web tests"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/dist-test/
    # Use cat to skip in Release mode
    COMMAND ${CMAKE_COMMAND} -E "$<IF:$<CONFIG:Debug,RelWithDebInfo>,${separate_dwarf_copy_args_debug},cat;-->"
    COMMAND_EXPAND_LISTS
    VERBATIM
    DEPENDS ${PROJECT_NAME}Weblib
)

add_custom_target(pep-repo-client-lib-web-tests-build
    COMMENT "Type-check & compile to JS for pep-repo-client-lib web tests"
    COMMAND ${NPM_PROGRAM} run build-web-tests
    VERBATIM
    DEPENDS ${PROJECT_NAME}WeblibJs
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_target_properties(pep-repo-client-lib-build PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/dist/"
)

add_custom_target(${PROJECT_NAME}WeblibJsWebTests ALL
    COMMENT "Build full JS weblib"
    DEPENDS
      pep-repo-client-lib-web-tests-build
      pep-repo-client-lib-separate-dwarf
)

if(PEP_WEBLIB_SOURCE_MAP)
  add_custom_target(pep-repo-client-lib-source-map
      COMMENT "Copy -gsource-map for pep-repo-client-lib web tests"
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/dist-test/
      COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE_DIR:${PROJECT_NAME}Weblib>/${PROJECT_NAME}Weblib.wasm.map
        ${CMAKE_CURRENT_SOURCE_DIR}/dist-test/${PROJECT_NAME}Weblib.wasm.map
      COMMAND_EXPAND_LISTS
      VERBATIM
      DEPENDS ${PROJECT_NAME}Weblib
  )
  add_dependencies(${PROJECT_NAME}WeblibJs pep-repo-client-lib-source-map)
endif()


configure_executable_companion(${PEP_INFRA_DIR}/client/ClientConfig.json REPLACE_VARIABLES)
configure_executable_companion(${PEP_INFRA_DIR}/rootCA.cert OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/ShadowAdministration.pub OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/../OAuthTokenSecret.json OPTIONAL)
if(NOT EXISTS "${PEP_INFRA_DIR}/rootCA.cert")
  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(TARGET weblib_repo_client_lib_pki FILES rootCA.cert)
  add_dependencies(${PROJECT_NAME}WeblibJs weblib_repo_client_lib_pki)
endif()
