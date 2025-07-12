#pragma once

#include <pep/networking/EndPoint.hpp>
#include <pep/utils/PropertySerializer.hpp>

namespace pep {

template <>
class PropertySerializer<EndPoint> : public PropertySerializerByValue<EndPoint> {
public:
  void write(boost::property_tree::ptree& destination, const EndPoint& value) const override;
  EndPoint read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
};

}
