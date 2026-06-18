#include <pep/utils/Configuration.hpp>

#include <boost/property_tree/json_parser.hpp>

namespace pep {


Configuration Configuration::FromFile(const std::filesystem::path& filepath) {
  Configuration result;

  std::filesystem::path abs = std::filesystem::canonical(filepath);
  boost::property_tree::read_json(abs.string(), result.properties_);

  result.setBasePath(abs.parent_path());

  return result;
}

Configuration Configuration::FromStream(
    std::istream& stream,
    const std::optional<std::filesystem::path>& basePath) {
  Configuration result;

  boost::property_tree::read_json(stream, result.properties_);

  if (basePath) {
    result.setBasePath(std::filesystem::absolute(*basePath));
  }

  return result;
}

Configuration Configuration::FromPtree(
    const boost::property_tree::ptree& properties,
    const std::optional<std::filesystem::path>& basePath) {

  Configuration result;
  result.properties_ = properties;

  if (basePath) {
    result.setBasePath(std::filesystem::absolute(*basePath));
  }

  return result;
}

void Configuration::setBasePath(const std::filesystem::path& base) {
  assert(base.is_absolute());
  deserializationContext_.add(TaggedBaseDirectory(base));
}


Configuration Configuration::get_child(const boost::property_tree::ptree::path_type& path) const {
  Configuration result;
  result.properties_ = this->properties_.get_child(path);
  result.deserializationContext_ = this->deserializationContext_;
  return result;
}

std::optional<Configuration> Configuration::get_child_optional(const boost::property_tree::ptree::path_type& path) const {
  if (auto child = this->properties_.get_child_optional(path)) {
    Configuration result;
    result.properties_ = *child;
    result.deserializationContext_ = this->deserializationContext_;
    return result;
  }
  return {};
}


}
