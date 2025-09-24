#include <pep/castor/SurveyStep.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string SurveyStep::RELATIVE_API_ENDPOINT = "survey-form";
const std::string SurveyStep::EMBEDDED_API_NODE_NAME = "survey_forms";

SurveyStep::SurveyStep(std::shared_ptr<Survey> survey, JsonPtr json)
  : SimpleCastorChildObject<SurveyStep, Survey>(survey, json),
  mName(GetFromPtree<std::string>(*json, "survey_form_name")) { // TODO: check if this matches new API
}

}
}
