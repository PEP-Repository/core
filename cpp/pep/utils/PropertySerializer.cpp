#include <pep/utils/PropertySerializer.hpp>

using boost::property_tree::ptree;

namespace pep {

void PropertySerializer<ptree>::write(ptree &destination, const ptree &value) const {
  destination = value;
}
void PropertySerializer<ptree>::read(ptree &destination, const ptree &source, const DeserializationContext &) const {
  destination = source;
}

void PropertySerializer<std::filesystem::path>::write(ptree &destination, const std::filesystem::path &value) const {
  SerializeProperties(destination, value.string());
}
std::filesystem::path PropertySerializer<std::filesystem::path>::read(const ptree &source, const DeserializationContext &context) const {
  std::filesystem::path result = DeserializeProperties<std::string>(source, context);
  if (!result.empty() && result.is_relative()) {
    if (auto base = context.get_value<TaggedBaseDirectory>()) {
      result = *base / result;
    }
  }
  return result;
}

}
