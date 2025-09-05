#include <pep/castor/OptionGroup.hpp>
#include <pep/castor/Ptree.hpp>

namespace pep{
namespace castor {

const std::string OptionGroup::RELATIVE_API_ENDPOINT = "field-optiongroup";
const std::string OptionGroup::EMBEDDED_API_NODE_NAME = "fieldOptionGroups";

OptionGroup::OptionGroup(std::shared_ptr<Study> study, JsonPtr json)
  : SimpleCastorChildObject(study, json), mName(GetFromPtree<std::string>(*json, "name")) {
  const auto& optionsJson = GetFromPtree<boost::property_tree::ptree>(*json, "options");
  for (const auto& entry : optionsJson) {
    const auto& option = entry.second;
    auto value = GetFromPtree<std::string>(option, "value");
    auto name = GetFromPtree<std::string>(option, "name");
    mOptions[value] = name;
  }
}

}
}
