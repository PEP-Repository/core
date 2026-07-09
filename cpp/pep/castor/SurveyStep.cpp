#include <pep/castor/SurveyStep.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string SurveyStep::RelativeApiEndpoint = "survey-form";
const std::string SurveyStep::EmbeddedApiNodeName = "survey_forms";

SurveyStep::SurveyStep(std::shared_ptr<Survey> survey, JsonPtr json)
  : SimpleCastorChildObject<SurveyStep, Survey>(survey, json),
  name_(GetFromPtree<std::string>(*json, "survey_form_name")) { // TODO: check if this matches new API
}

}
}
