set(PEPCLI_SRCS
    Ama.cpp
    user/CommandUserGroup.cpp user/CommandUserGroup.hpp
    user/UserSubCommands.cpp user/UserSubCommands.hpp
    user/CommandUserQuery.cpp user/CommandUserQuery.hpp
    Token.cpp Token.hpp
    token/CommandTokenBlock.cpp token/CommandTokenBlock.hpp
    User.cpp User.hpp
    Castor.cpp
    CliApplication.cpp CliApplication.hpp
    Command.cpp Command.hpp
    Commands.hpp
    Delete.cpp
    DownloadDirectory.cpp DownloadDirectory.hpp
    DownloadMetadata.cpp DownloadMetadata.hpp
    DownloadMetadata.PropertySerializers.cpp DownloadMetadata.PropertySerializers.hpp
    DownloadProcessor.cpp DownloadProcessor.hpp
    Export.cpp Export.hpp
    export/CommandExportCsv.cpp export/CommandExportCsv.hpp
    export/CommandExportJson.cpp export/CommandExportJson.hpp
    export/CommandExportYaml.cpp export/CommandExportYaml.hpp
    FileExtension.cpp
    Get.cpp
    History.cpp
    List.cpp
    Metrics.cpp
    MultiCellQuery.cpp MultiCellQuery.hpp
    NoLongerSupportedCommand.cpp
    Ping.cpp
    Pull.cpp
    Query.cpp
    Register.cpp
    SingleCellCommand.cpp SingleCellCommand.hpp
    Store.cpp
    structuredoutput/TableFromDownloadDirectory.cpp structuredoutput/TableFromDownloadDirectory.hpp
    StructureMetadata.cpp
    TicketFile.cpp TicketFile.hpp
    Validate.cpp
    Verifiers.cpp
    XEntry.cpp
)

add_cli_install_executable(${PROJECT_NAME}cli ${PEPCLI_SRCS})

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    find_package(Sparkle REQUIRED)

    if(NOT SPARKLE_LIBRARY)
        message(FATAL_ERROR "Sparkle library not found")
    endif()

    # Get the directory of the Sparkle library
    get_filename_component(SPARKLE_DIR ${SPARKLE_LIBRARY} DIRECTORY)

    # Append the relative path to the Sparkle binary
    set(SPARKLE_BINARY "${SPARKLE_DIR}/sparkle.app/Contents/MacOS/sparkle")

    #TODO cleanup using --install approach as used on Windows?
    add_custom_command(TARGET ${PROJECT_NAME}cli POST_BUILD
        COMMENT "Copy Sparkle framework files to cli"
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}cli>/Frameworks/
        COMMAND cp -R ${SPARKLE_LIBRARY} $<TARGET_FILE_DIR:${PROJECT_NAME}cli>/Frameworks/

        # Copy the Sparkle binary next to the target file
        COMMAND cp ${SPARKLE_BINARY} $<TARGET_FILE_DIR:${PROJECT_NAME}cli>
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(${PROJECT_NAME}cli PRIVATE VersionInfo.rc)
endif()

target_link_libraries(${PROJECT_NAME}cli
  ${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Archivinglib
  ${PROJECT_NAME}Clientlib
  ${PROJECT_NAME}Morphinglib
  ${PROJECT_NAME}StructuredOutputlib
)
if(WITH_CASTOR)
    target_link_libraries(${PROJECT_NAME}cli ${PROJECT_NAME}Castorlib)
endif()

configure_executable_companion(${PEP_INFRA_DIR}/client/ClientConfig.json REPLACE_VARIABLES)
configure_executable_companion(${PEP_INFRA_DIR}/client/ClientKeys.json OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/ShadowAdministration.pub OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/rootCA.cert OPTIONAL)
configure_executable_config_version(${PROJECT_NAME}cli local)
# Only copy generated PKI file if predefined one does not exist for this infrastructure
if(NOT EXISTS "${PEP_INFRA_DIR}/rootCA.cert")
  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(TARGET cli_pki FILES rootCA.cert)
  add_dependencies(${PROJECT_NAME}cli cli_pki)
endif()

configure_debug_command_line_parameter(${PROJECT_NAME}cli --client-working-directory ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE})
