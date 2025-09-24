#include <pep/elgamal/CurvePoint.PropertySerializer.hpp>

namespace pep {

void PropertySerializer<CurvePoint>::write(boost::property_tree::ptree& destination, const CurvePoint& value) const {
  SerializeProperties(destination, value.text());
}

CurvePoint PropertySerializer<CurvePoint>::read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const {
  return CurvePoint::FromText(DeserializeProperties<std::string>(source, transform));
}

}
