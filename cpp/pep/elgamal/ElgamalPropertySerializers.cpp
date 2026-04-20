#include <pep/elgamal/ElgamalPropertySerializers.hpp>

namespace pep {

void PropertySerializer<CurvePoint>::write(boost::property_tree::ptree& destination, const CurvePoint& value) const {
  SerializeProperties(destination, value.text());
}
CurvePoint PropertySerializer<CurvePoint>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  return CurvePoint::FromText(DeserializeProperties<std::string>(source, context));
}

void PropertySerializer<CurveScalar>::write(boost::property_tree::ptree& destination, const CurveScalar& value) const {
  SerializeProperties(destination, value.text());
}
CurveScalar PropertySerializer<CurveScalar>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  return CurveScalar::FromText(DeserializeProperties<std::string>(source, context));
}

}
