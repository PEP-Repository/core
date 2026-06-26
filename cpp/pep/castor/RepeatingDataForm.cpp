#include <pep/castor/RepeatingDataForm.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep {
namespace castor {

const std::string RepeatingDataForm::RelativeApiEndpoint = "repeating-data-form";
const std::string RepeatingDataForm::EmbeddedApiNodeName = "repeating_data_forms";

RepeatingDataForm::RepeatingDataForm(std::shared_ptr<RepeatingData> repeatingData, JsonPtr json)
  : SimpleCastorChildObject<RepeatingDataForm, RepeatingData>(repeatingData, json),
  name_(GetFromPtree<std::string>(*json, "repeating_data_form_name")),
  number_(GetFromPtree<int>(*json, "repeating_data_form_number")) {}

}
}
