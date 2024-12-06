#include <pep/castor/RepeatingDataForm.hpp>

namespace pep {
namespace castor {

const std::string RepeatingData::RELATIVE_API_ENDPOINT = "repeating-data";
const std::string RepeatingData::EMBEDDED_API_NODE_NAME = "repeatingData";

rxcpp::observable<std::shared_ptr<RepeatingDataForm>> RepeatingData::getRepeatingDataForms() {
  return RepeatingDataForm::RetrieveForParent(SharedFrom(*this));
}

}
}
