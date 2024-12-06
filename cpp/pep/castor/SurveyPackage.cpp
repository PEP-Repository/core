#include <pep/castor/Survey.hpp>
#include <pep/castor/SurveyPackage.hpp>

#include <pep/async/RxUtils.hpp>

namespace pep {
namespace castor {

const std::string SurveyPackage::RELATIVE_API_ENDPOINT = "surveypackage";
const std::string SurveyPackage::EMBEDDED_API_NODE_NAME = "survey_packages";

std::string SurveyPackage::getName() const {
  return mName;
}

rxcpp::observable<std::shared_ptr<Survey>> SurveyPackage::getSurveys() const {
  return rxcpp::observable<>::iterate(mSurveysJson)
    .map([self = SharedFrom(*this)](auto item) { return Survey::Create(self->getParent(), std::make_shared<boost::property_tree::ptree>(item.second)); });
}

}
}
