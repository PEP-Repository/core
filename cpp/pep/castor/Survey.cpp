#include <pep/castor/SurveyStep.hpp>
#include <pep/castor/Ptree.hpp>
#include <pep/castor/CastorConnection.hpp> // for castor::JsonPtr
#include <pep/castor/CastorObject.hpp> // for castor::SimpleCastorChildObject
#include <pep/castor/Study.hpp>
#include <pep/castor/Survey.hpp>
#include <pep/utils/Shared.hpp>

#include <rxcpp/rx-lite.hpp>

#include <memory>
#include <string>

namespace pep {
namespace castor {

const std::string Survey::RelativeApiEndpoint = "survey";
const std::string Survey::EmbeddedApiNodeName = "surveys";

Survey::Survey(std::shared_ptr<Study> study, JsonPtr json)
  : SimpleCastorChildObject<Survey, Study>(study, json), name_(GetFromPtree<std::string>(*json, "name")) {
}

std::string Survey::getName() const {
  return name_;
}

rxcpp::observable<std::shared_ptr<SurveyStep>> Survey::getSteps() {
  return SurveyStep::RetrieveForParent(SharedFrom(*this));
}

}
}
