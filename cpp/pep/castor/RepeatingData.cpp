#include <pep/castor/RepeatingDataForm.hpp>
#include <pep/castor/Ptree.hpp>
#include <pep/castor/CastorConnection.hpp> // for castor::JsonPtr
#include <pep/castor/CastorObject.hpp> // for castor::SimpleCastorChildObject
#include <pep/castor/RepeatingData.hpp>
#include <pep/castor/Study.hpp>
#include <pep/utils/Shared.hpp>

#include <rxcpp/rx-lite.hpp>

#include <memory>
#include <string>

namespace pep {
namespace castor {

const std::string RepeatingData::RelativeApiEndpoint = "repeating-data";
const std::string RepeatingData::EmbeddedApiNodeName = "repeatingData";

RepeatingData::RepeatingData(std::shared_ptr<Study> study, JsonPtr json)
  : SimpleCastorChildObject<RepeatingData, Study>(study, json), name_(GetFromPtree<std::string>(*json, "name")) {
}

rxcpp::observable<std::shared_ptr<RepeatingDataForm>> RepeatingData::getRepeatingDataForms() {
  return RepeatingDataForm::RetrieveForParent(SharedFrom(*this));
}

}
}
