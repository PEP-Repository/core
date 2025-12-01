#include <pep/networking/EndPoint.PropertySerializer.hpp>

namespace pep {

void PropertySerializer<EndPoint>::write(boost::property_tree::ptree& destination, const EndPoint& value) const {
  SerializeProperties(destination, "Address", value.hostname);
  SerializeProperties(destination, "Port", value.port);
  SerializeProperties(destination, "Name", value.expectedCommonName);
}

EndPoint PropertySerializer<EndPoint>::read(const boost::property_tree::ptree& source, const DeserializationContext& context) const {
  auto hostname = DeserializeProperties<std::string>(source, "Address", context);
  auto port = DeserializeProperties<uint16_t>(source, "Port", context);
  auto result = EndPoint(hostname, port);

  auto expectedCommonNameOp = DeserializeProperties<std::optional<std::string>>(
    source, "Name", context);

  if (expectedCommonNameOp.has_value())
    result.expectedCommonName = *expectedCommonNameOp;

  return result;
}

}
