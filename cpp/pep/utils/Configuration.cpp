#include <pep/utils/Configuration.hpp>

#include <ranges>

#include <boost/property_tree/json_parser.hpp>
#include <pep/utils/CollectionUtils.hpp>

using namespace std::ranges;

namespace pep {
Configuration::Configuration(
  const boost::property_tree::ptree &properties,
  DeserializationContext deserialization_context)
  : properties_(properties),
    deserializationContext_(std::move(deserialization_context)) {}

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

std::unordered_map<std::string, Configuration> Configuration::get_children_map(const boost::property_tree::ptree::path_type& path) const {
  return RangeToCollection<std::unordered_map<std::string, Configuration>>(
    DeserializeProperties<std::unordered_map<std::string, boost::property_tree::ptree>>(properties_, path, deserializationContext_)
    | views::transform([&](const auto& entry) {
      return std::pair{entry.first, Configuration(entry.second, deserializationContext_)};
    })
  );
}

std::vector<Configuration> Configuration::get_children_vector(const boost::property_tree::ptree::path_type& path) const {
  return RangeToVector(
    DeserializeProperties<std::vector<boost::property_tree::ptree>>(properties_, path, deserializationContext_)
    | views::transform([&](const boost::property_tree::ptree& tree) {
      return Configuration(tree, deserializationContext_);
    })
  );
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
