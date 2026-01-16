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
    COMMENT "Install dependencies"
    COMMAND ${NPM_PROGRAM} install
    VERBATIM
    DEPENDS pep-repo-client-wasm-symlink
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_target_properties(pep-repo-client-lib-npm PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/"
)

add_custom_target(pep-repo-client-lib-build
    COMMENT "Type-check & compile to JS"
    COMMAND ${NPM_PROGRAM} run build
    VERBATIM
    DEPENDS pep-repo-client-lib-npm
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)
set_target_properties(pep-repo-client-lib-build PROPERTIES
    ADDITIONAL_CLEAN_FILES "${CMAKE_CURRENT_SOURCE_DIR}/dist/"
)

add_custom_target(pep-repo-client-lib-config
    COMMENT "Copy config to test_config folder"
    COMMAND ${CMAKE_COMMAND} -E copy
      ClientConfig.json
      rootCA.cert
      ShadowAdministration.pub
      OAuthTokenSecret.json
      "${CMAKE_CURRENT_SOURCE_DIR}/test_config/"
    VERBATIM
    DEPENDS
      ClientConfig.json
      rootCA.cert
      ShadowAdministration.pub
      OAuthTokenSecret.json
    BYPRODUCTS
      "${CMAKE_CURRENT_SOURCE_DIR}/test_config/ClientConfig.json"
      "${CMAKE_CURRENT_SOURCE_DIR}/test_config/rootCA.cert"
      "${CMAKE_CURRENT_SOURCE_DIR}/test_config/ShadowAdministration.pub"
      "${CMAKE_CURRENT_SOURCE_DIR}/test_config/OAuthTokenSecret.json"
)

add_custom_target(${PROJECT_NAME}WeblibJs ALL
    COMMENT "Build full JS weblib"
    DEPENDS
      pep-repo-client-lib-build
      pep-repo-client-lib-config
)

configure_executable_companion(${PEP_INFRA_DIR}/client/ClientConfig.json REPLACE_VARIABLES)
configure_executable_companion(${PEP_INFRA_DIR}/rootCA.cert OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/ShadowAdministration.pub OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/../OAuthTokenSecret.json OPTIONAL)
if(NOT EXISTS "${PEP_INFRA_DIR}/rootCA.cert")
  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(TARGET weblib_repo_client_lib_pki FILES rootCA.cert)
  add_dependencies(${PROJECT_NAME}WeblibJs weblib_repo_client_lib_pki)
endif()
