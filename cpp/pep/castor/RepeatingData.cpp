#include <pep/castor/RepeatingDataForm.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string RepeatingData::RELATIVE_API_ENDPOINT = "repeating-data";
const std::string RepeatingData::EMBEDDED_API_NODE_NAME = "repeatingData";

RepeatingData::RepeatingData(std::shared_ptr<Study> study, JsonPtr json)
  : SimpleCastorChildObject<RepeatingData, Study>(study, json), mName(GetFromPtree<std::string>(*json, "name")) {
}

rxcpp::observable<std::shared_ptr<RepeatingDataForm>> RepeatingData::getRepeatingDataForms() {
  return RepeatingDataForm::RetrieveForParent(SharedFrom(*this));
}

}
}
