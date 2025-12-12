#pragma once

#include <pep/assessor/Branding.hpp>
#include <pep/utils/PropertySerializer.hpp>

namespace pep {

  template <>
  class PropertySerializer<BrandingConfiguration> : public PropertySerializerByReference<BrandingConfiguration> {
  public:
    void write(boost::property_tree::ptree& destination, const BrandingConfiguration& value) const override;
    void read(BrandingConfiguration& destination, const boost::property_tree::ptree& source, const DeserializationContext& context) const override;
  };

}
