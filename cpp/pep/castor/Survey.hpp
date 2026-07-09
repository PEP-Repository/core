#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class SurveyStep;

class Survey : public SimpleCastorChildObject<Survey, Study>, public SharedConstructor<Survey> {
private:
  std::string name_;

public:
  static const std::string RelativeApiEndpoint;
  static const std::string EmbeddedApiNodeName;

  //! \return The full name of the survey
  std::string getName() const;

  rxcpp::observable<std::shared_ptr<SurveyStep>> getSteps();

protected:
  Survey(std::shared_ptr<Study> study, JsonPtr json);

private:
  friend class SharedConstructor<Survey>;
};

}
}
