#pragma once

#include <pep/elgamal/CurvePoint.hpp>
#include <pep/utils/PropertySerializer.hpp>

namespace pep {

template <>
class PropertySerializer<CurvePoint> : public PropertySerializerByValue<CurvePoint> {
public:
  void write(boost::property_tree::ptree& destination, const CurvePoint& value) const override;
  CurvePoint read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
};

template <>
class PropertySerializer<CurveScalar> : public PropertySerializerByValue<CurveScalar> {
public:
  void write(boost::property_tree::ptree& destination, const CurveScalar& value) const override;
  CurveScalar read(const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
};

}
