#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class SurveyPackage : public SimpleCastorChildObject<SurveyPackage, Study>, public SharedConstructor<SurveyPackage> {
private:
  std::string mName;
  boost::property_tree::ptree mSurveysJson;

public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

  std::string getName() const;

  rxcpp::observable<std::shared_ptr<Survey>> getSurveys() const;

protected:
  SurveyPackage(std::shared_ptr<Study> study, JsonPtr json)
    : SimpleCastorChildObject<SurveyPackage, Study>(study, json),
    mName(GetFromPtree<std::string>(*json, "name")),
    mSurveysJson(GetFromPtree<boost::property_tree::ptree>(*json, "_embedded.surveys")) { // Documented on https://data.castoredc.com/api#/survey/get_study__study_id__surveypackage as "_embbeded" (note the typo) and without a child node
  }

private:
  friend class SharedConstructor<SurveyPackage>;
};

}
}
