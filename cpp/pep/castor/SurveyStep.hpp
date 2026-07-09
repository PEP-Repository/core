#pragma once

#include <pep/castor/Survey.hpp>

namespace pep {
namespace castor {

class SurveyStep : public SimpleCastorChildObject<SurveyStep, Survey>, public SharedConstructor<SurveyStep> {
 private:
  std::string name_;

 public:
  static const std::string RelativeApiEndpoint;
  static const std::string EmbeddedApiNodeName;

  std::string getName() const { return name_; }
  std::shared_ptr<Survey> getSurvey() const { return this->getParent(); }

 protected:
  /*!
   * \brief construct a Step
   *
   * \param survey The Survey this step belongs to
   * \param json The %Json response from the Castor API for this step
   */
   SurveyStep(std::shared_ptr<Survey> survey, JsonPtr json);

 private:
  friend class SharedConstructor<SurveyStep>;
};

}
}
