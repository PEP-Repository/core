#include <pep/rsk-pep/Pseudonyms.PropertySerializers.hpp>

using namespace pep;

void PropertySerializer<LocalPseudonym>::write(boost::property_tree::ptree& destination, const LocalPseudonym& value) const {
  SerializeProperties(destination, value.text());
}

LocalPseudonym PropertySerializer<LocalPseudonym>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  return LocalPseudonym::FromText(DeserializeProperties<std::string>(source, transform));
}

void PropertySerializer<EncryptedLocalPseudonym>::write(boost::property_tree::ptree& destination, const EncryptedLocalPseudonym& value) const {
  SerializeProperties(destination, value.text());
}

EncryptedLocalPseudonym PropertySerializer<EncryptedLocalPseudonym>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  return EncryptedLocalPseudonym::FromText(DeserializeProperties<std::string>(source, transform));
}

void PropertySerializer<PolymorphicPseudonym>::write(boost::property_tree::ptree& destination, const PolymorphicPseudonym& value) const {
  SerializeProperties(destination, value.text());
}

PolymorphicPseudonym PropertySerializer<PolymorphicPseudonym>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  return PolymorphicPseudonym::FromText(DeserializeProperties<std::string>(source, transform));
}
