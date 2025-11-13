add_cli_install_executable(${PROJECT_NAME}PullCastor
    CastorParticipant.cpp CastorParticipant.hpp
    CellContent.cpp CellContent.hpp
    ColumnBoundParticipantId.cpp ColumnBoundParticipantId.hpp
    CrfAspectPuller.cpp CrfAspectPuller.hpp
    EnvironmentPuller.cpp EnvironmentPuller.hpp
    FieldValue.cpp FieldValue.hpp
    main.cpp
    Metrics.cpp Metrics.hpp
    PepParticipant.cpp PepParticipant.hpp
    PullCastorUtils.cpp PullCastorUtils.hpp
    RepeatingDataAspectPuller.cpp RepeatingDataAspectPuller.hpp
    RepeatingDataPuller.cpp RepeatingDataPuller.hpp
    StorableContent.cpp StorableContent.hpp
    StoredData.cpp StoredData.hpp
    StudyAspect.cpp StudyAspect.hpp
    StudyAspectPuller.cpp StudyAspectPuller.hpp
    StudyPuller.cpp StudyPuller.hpp
    SurveyAspectPuller.cpp SurveyAspectPuller.hpp
    SurveyPackageInstancePuller.cpp SurveyPackageInstancePuller.hpp
    TimestampedSpi.cpp TimestampedSpi.hpp
)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(${PROJECT_NAME}PullCastor PRIVATE VersionInfo.rc)
endif()

target_link_libraries(${PROJECT_NAME}PullCastor
  ${PROJECT_NAME}Castorlib
  ${PROJECT_NAME}Applicationlib
  ${PROJECT_NAME}Clientlib
  ${PROJECT_NAME}Metricslib
)

try_compile(HAS_CXX20_CHRONO
  SOURCE_FROM_CONTENT check_cxx20_chrono.cpp
  #language=c++
  [[
    #include <chrono>
    #if __cpp_lib_chrono < 201907L
    # error "C++20 chrono not fully supported"
    #endif
    int main() {}
  ]]
  LOG_DESCRIPTION "Check C++20 chrono support"
)
if(NOT HAS_CXX20_CHRONO)
  find_package(date REQUIRED)
  target_link_libraries(${PROJECT_NAME}PullCastor
    date::date
  )
endif()

configure_executable_companion(${PEP_INFRA_DIR}/client/PullCastor.json OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/client/ClientConfig.json OPTIONAL)
configure_executable_companion(${PEP_INFRA_DIR}/rootCA.cert OPTIONAL)
configure_executable_config_version(${PROJECT_NAME}PullCastor infra)
if (DEFINED CASTOR_API_KEY_DIR)
    configure_executable_companion("${CASTOR_API_KEY_DIR}/CastorAPIKey.json")
endif()

if(PEP_LOCAL_INFRA)
  add_custom_command(TARGET ${PROJECT_NAME}PullCastor POST_BUILD
    COMMENT "Generate PullCastor token"
    COMMAND ${PROJECT_NAME}Token --json --secret-json ${PEP_LOCAL_INFRA_DIR}/../OAuthTokenSecret.json --subject PullCastor --group PullCastor > ${CMAKE_CURRENT_BINARY_DIR}/OAuthToken.json)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/oauthTokens/PullCastor.json")
  configure_file("${CMAKE_SOURCE_DIR}/oauthTokens/PullCastor.json" "${CMAKE_CURRENT_BINARY_DIR}/OAuthToken.json" COPYONLY)
  # TODO else remove file that may have remained from previous invocation
endif()

if("${CMAKE_GENERATOR}" MATCHES "Visual Studio.*")
    # The "COPY" command can't deal with forward slashes :-(
    string(REPLACE "/" "\\" CMAKE_WINDOWS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
    # Ensure that the OAuthToken.json file is (also) present in the executable directories (where PullCastor.json is located).
    foreach(config ${CMAKE_CONFIGURATION_TYPES})
        add_custom_command(TARGET ${PROJECT_NAME}PullCastor POST_BUILD
            COMMENT "Copy PullCastor token"
            COMMAND IF EXIST "${CMAKE_WINDOWS_BINARY_DIR}\\OAuthToken.json" COPY /Y "${CMAKE_WINDOWS_BINARY_DIR}\\OAuthToken.json" "${CMAKE_WINDOWS_BINARY_DIR}\\${config}\\")
    endforeach()

    configure_debug_command_line_parameter(${PROJECT_NAME}PullCastor --dry ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/PullCastor.json)
endif()

if(NOT EXISTS "${PEP_INFRA_DIR}/rootCA.cert")
  include(${PROJECT_SOURCE_DIR}/pki/pki.cmake)
  copy_pki_files(TARGET pullcastor_pki FILES rootCA.cert)
  add_dependencies(${PROJECT_NAME}PullCastor pullcastor_pki)
endif()
