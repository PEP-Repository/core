#pragma once

#include <pep/morphing/EnrolledPartyKeys.hpp>
#include <pep/morphing/SystemPublicKeys.hpp>
#include <pep/utils/PropertySerializer.hpp>

namespace pep {

template <>
class PropertySerializer<EnrolledPartyKeys> : public PropertySerializerByValue<EnrolledPartyKeys> {
public:
  void write(boost::property_tree::ptree& destination, const EnrolledPartyKeys& value) const override;
  EnrolledPartyKeys read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
};

template <>
class PropertySerializer<SystemPublicKeys> : public PropertySerializerByValue<SystemPublicKeys> {
public:
  void write(boost::property_tree::ptree& destination, const SystemPublicKeys& value) const override;
  SystemPublicKeys read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
};

}
