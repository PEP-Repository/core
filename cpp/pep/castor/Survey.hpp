#pragma once

#include <pep/castor/Study.hpp>

namespace pep {
namespace castor {

class SurveyStep;

class Survey : public SimpleCastorChildObject<Survey, Study>, public SharedConstructor<Survey> {
private:
  std::string mName;

public:
  static const std::string RELATIVE_API_ENDPOINT;
  static const std::string EMBEDDED_API_NODE_NAME;

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
