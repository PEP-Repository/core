#pragma once

#include <pep/rsk-pep/Pseudonyms.hpp>
#include <pep/utils/PropertySerializer.hpp>

namespace pep {

template <>
class PropertySerializer<LocalPseudonym> final : public PropertySerializerByValue<LocalPseudonym> {
public:
  void write(boost::property_tree::ptree& destination, const LocalPseudonym& value) const override;
  LocalPseudonym read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
};

template <>
class PropertySerializer<EncryptedLocalPseudonym> final : public PropertySerializerByValue<EncryptedLocalPseudonym> {
public:
  void write(boost::property_tree::ptree& destination, const EncryptedLocalPseudonym& value) const override;
  EncryptedLocalPseudonym read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
};

template <>
class PropertySerializer<PolymorphicPseudonym> final : public PropertySerializerByValue<PolymorphicPseudonym> {
public:
  void write(boost::property_tree::ptree& destination, const PolymorphicPseudonym& value) const override;
  PolymorphicPseudonym read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
};

} // namespace pep
