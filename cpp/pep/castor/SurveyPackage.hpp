#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class SurveyPackage : public SimpleCastorChildObject<SurveyPackage, Study>, public SharedConstructor<SurveyPackage> {
private:
  std::string name_;
  std::unique_ptr<boost::property_tree::ptree> surveysJson_;

public:
  static const std::string RelativeApiEndpoint;
  static const std::string EmbeddedApiNodeName;

  std::string getName() const;

  rxcpp::observable<std::shared_ptr<Survey>> getSurveys() const;

protected:
  SurveyPackage(std::shared_ptr<Study> study, JsonPtr json);

private:
  friend class SharedConstructor<SurveyPackage>;
};

}
}
