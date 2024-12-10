#pragma once

#include <pep/utils/PropertySerializer.hpp>

#include <cstdint>

namespace pep {

struct EndPoint {
  std::string hostname;
  uint16_t port;
  std::string expectedCommonName;

  EndPoint() = default;
  EndPoint(
      std::string hostname,
      uint16_t port,
      std::string expectedCommonName = "")
    : hostname(std::move(hostname)), port{port}, expectedCommonName(std::move(expectedCommonName))
  {}

  std::string Describe() const {
    return this->expectedCommonName.empty() 
      ? this->hostname 
      : this->expectedCommonName;
  }
};

template <>
class PropertySerializer<EndPoint> : public PropertySerializerByValue<EndPoint> {
public:
  void write(boost::property_tree::ptree& destination, const EndPoint& value) const override;
  EndPoint read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override;
};

}
