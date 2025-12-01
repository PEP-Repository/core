#include <pep/assessor/Branding.PropertySerializer.hpp>

namespace pep {

void PropertySerializer<BrandingConfiguration>::write(boost::property_tree::ptree& destination, const BrandingConfiguration& value) const {
  SerializeProperties(destination, "ProjectName", value.projectName);
  SerializeProperties(destination, "ProjectLogo", value.projectLogo);
}

void PropertySerializer<BrandingConfiguration>::read(BrandingConfiguration& destination, const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  DeserializeProperties(destination.projectName, source, "ProjectName", context);
  DeserializeProperties(destination.projectLogo, source, "ProjectLogo", context);
}

}
