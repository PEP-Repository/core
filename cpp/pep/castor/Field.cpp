#include <pep/castor/Field.hpp>
#include <pep/castor/OptionGroup.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep{
namespace castor {

namespace {

const std::string RELATIVE_API_ENDPOINT = "field";
const std::string EMBEDDED_API_NODE_NAME = "fields";

}

const std::string Field::TYPE_CHECKBOX = "checkbox";
const std::string Field::TYPE_REPEATED_MEASURE = "repeated_measures";

std::string Field::makeUrl() const {
  return this->makeParentRelativeUrl(RELATIVE_API_ENDPOINT);
}

Field::Field(std::shared_ptr<Study> study, JsonPtr json)
  : ParentedCastorObject<Study>(study, json),
  mParentId(GetFromPtree<std::string>(*json, "parent_id")),
  mNumber(GetFromPtree<int>(*json, "field_number")),
  mType(GetFromPtree<std::string>(*json, "field_type")),
  mVariableName(GetFromPtree<std::string>(*json, "field_variable_name")),
  mLabel(GetFromPtree<std::string>(*json, "field_label")),
  mRequired(GetFromPtree<bool>(*json, "field_required")),
  mUnits(GetFromPtree<std::string>(*json, "field_units")),
  mInfo(GetFromPtree<std::string>(*json, "field_info")),
  mHidden(GetFromPtree<bool>(*json, "field_hidden")),
  mReportId(GetFromPtree<std::string>(*json, "report_id")) {

  if (auto optionGroupJson = GetFromPtree<boost::optional<boost::property_tree::ptree>>(*json, "option_group")) {
    mOptionGroup = OptionGroup::Create(this->getParent(), std::make_shared<boost::property_tree::ptree>(*optionGroupJson));
  }

}

rxcpp::observable<std::shared_ptr<Field>> Field::RetrieveForParent(std::shared_ptr<Study> study) {
  return CastorObject::RetrieveList<Field, Study>(study, GetParentRelativeEndpoint(study, RELATIVE_API_ENDPOINT + "?include=optiongroup"), EMBEDDED_API_NODE_NAME);
}


}
}
