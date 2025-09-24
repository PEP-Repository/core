#include <pep/castor/OptionGroup.hpp>
#include <pep/pullcastor/FieldValue.hpp>

#include <boost/algorithm/string/split.hpp>
#include <boost/property_tree/ptree.hpp>

namespace pep {
namespace castor {

namespace {

void AddCheckBox(boost::property_tree::ptree& destination, const std::string& name, const std::string& value, std::shared_ptr<OptionGroup> optionGroup) {
  if (optionGroup == nullptr) {
    throw std::runtime_error("No OptionGroup specified for checkbox field '" + name + "'");
  }

  std::set<std::string> selectedValues;
  boost::split(selectedValues, value, [](char c) { return c == ';'; });
  auto unfound = selectedValues.cend();

  using ptree_path = boost::property_tree::ptree::path_type;
  ptree_path root(name);

  for (const auto& option : optionGroup->getOptions()) {
    auto selected = (selectedValues.find(option.first) != unfound);
    destination.put(root / ptree_path(option.first), selected);
  }
}

}

FieldValue::FieldValue(std::shared_ptr<Field> field, std::shared_ptr<DataPointBase> dataPoint)
  : mField(field), mDataPoint(dataPoint) {
}

void FieldValue::addTo(boost::property_tree::ptree& destination) const {
  auto type = mField->getType();
  auto name = mField->getVariableName();

  std::string value;
  if (mDataPoint != nullptr) {
    value = mDataPoint->getValue();
  }

  if (type == Field::TYPE_CHECKBOX) {
    AddCheckBox(destination, name, value, mField->getOptionGroup());
  }
  else {
    destination.put(name, value);
  }
}

rxcpp::observable<std::shared_ptr<boost::property_tree::ptree>> FieldValue::Aggregate(rxcpp::observable<std::shared_ptr<FieldValue>> values) {
  return values
    .reduce(
      std::make_shared<boost::property_tree::ptree>(),
      [](std::shared_ptr<boost::property_tree::ptree> result, std::shared_ptr<FieldValue> value) {
        value->addTo(*result);
        return result;
      }
    );
}

}
}
