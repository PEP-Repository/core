#include <pep/castor/SurveyStep.hpp>

namespace pep {
namespace castor {

const std::string Survey::RELATIVE_API_ENDPOINT = "survey";
const std::string Survey::EMBEDDED_API_NODE_NAME = "surveys";

std::string Survey::getName() const {
  return mName;
}

rxcpp::observable<std::shared_ptr<SurveyStep>> Survey::getSteps() {
  return SurveyStep::RetrieveForParent(SharedFrom(*this));
}

}
}
