#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class SurveyPackage : public SimpleCastorChildObject<SurveyPackage, Study>, public SharedConstructor<SurveyPackage> {
private:
  std::string mName;
  std::unique_ptr<boost::property_tree::ptree> mSurveysJson;

public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

  std::string getName() const;

  rxcpp::observable<std::shared_ptr<Survey>> getSurveys() const;

protected:
  SurveyPackage(std::shared_ptr<Study> study, JsonPtr json);

private:
  friend class SharedConstructor<SurveyPackage>;
};

}
}
