#include <pep/castor/RepeatingDataForm.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string RepeatingDataForm::RELATIVE_API_ENDPOINT = "repeating-data-form";
const std::string RepeatingDataForm::EMBEDDED_API_NODE_NAME = "repeating_data_forms";

RepeatingDataForm::RepeatingDataForm(std::shared_ptr<RepeatingData> repeatingData, JsonPtr json)
  : SimpleCastorChildObject<RepeatingDataForm, RepeatingData>(repeatingData, json),
  mName(GetFromPtree<std::string>(*json, "repeating_data_form_name")),
  mNumber(GetFromPtree<int>(*json, "repeating_data_form_number")) {}

}
}
