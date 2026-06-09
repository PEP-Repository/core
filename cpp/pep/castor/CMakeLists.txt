set(CASTOR_SOURCES
    ApiKey.cpp ApiKey.hpp
    BulkRetrieveChildren.hpp
    CastorClient.cpp CastorClient.hpp
    CastorConnection.cpp CastorConnection.hpp
    CastorObject.cpp CastorObject.hpp
    DataPoint.cpp DataPoint.hpp
    Field.cpp Field.hpp
    Form.cpp Form.hpp
    ImportColumnNamer.cpp ImportColumnNamer.hpp
    OptionGroup.cpp OptionGroup.hpp
    Participant.cpp Participant.hpp
    Ptree.cpp Ptree.hpp
    RepeatingData.cpp RepeatingData.hpp
    RepeatingDataForm.cpp RepeatingDataForm.hpp
    RepeatingDataInstance.cpp RepeatingDataInstance.hpp
    RepeatingDataPoint.cpp RepeatingDataPoint.hpp
    Site.cpp Site.hpp
    Study.cpp Study.hpp
    StudyDataPoint.cpp StudyDataPoint.hpp
    Survey.cpp Survey.hpp
    SurveyDataPoint.cpp SurveyDataPoint.hpp
    SurveyPackage.cpp SurveyPackage.hpp
    SurveyPackageInstance.cpp SurveyPackageInstance.hpp
    SurveyStep.cpp SurveyStep.hpp
    Visit.cpp Visit.hpp
)

add_library(${PROJECT_NAME}Castorlib ${CASTOR_SOURCES})

find_package(Boost REQUIRED)
target_link_libraries(${PROJECT_NAME}Castorlib
  PUBLIC
    ${PROJECT_NAME}Structurelib
    ${PROJECT_NAME}Networkinglib
  PRIVATE
    Boost::date_time
    entities
)

verify_headers_standalone(${PROJECT_NAME}Castorlib)
if (WITH_TESTS)
  add_unit_tests(Castor)
endif ()
