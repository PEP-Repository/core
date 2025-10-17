#include <pep/castor/SurveyPackage.hpp>

#include <pep/async/RxMoveIterate.hpp>
#include <pep/castor/Survey.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string SurveyPackage::RELATIVE_API_ENDPOINT = "surveypackage";
const std::string SurveyPackage::EMBEDDED_API_NODE_NAME = "survey_packages";

SurveyPackage::SurveyPackage(std::shared_ptr<Study> study, JsonPtr json)
  : SimpleCastorChildObject<SurveyPackage, Study>(study, json),
  mName(GetFromPtree<std::string>(*json, "name")),
  mSurveysJson(std::make_unique<boost::property_tree::ptree>(GetFromPtree<boost::property_tree::ptree>(*json, "_embedded.surveys"))) { // Documented on https://data.castoredc.com/api#/survey/get_study__study_id__surveypackage as "_embbeded" (note the typo) and without a child node
}

std::string SurveyPackage::getName() const {
  return mName;
}

rxcpp::observable<std::shared_ptr<Survey>> SurveyPackage::getSurveys() const {
  return RxMoveIterate(*mSurveysJson)
    .map([self = SharedFrom(*this)](auto item) { return Survey::Create(self->getParent(), std::make_shared<boost::property_tree::ptree>(std::move(item.second))); });
}

}
}
