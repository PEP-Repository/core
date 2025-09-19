#include <pep/utils/Configuration.hpp>

#include <boost/property_tree/json_parser.hpp>

namespace pep {


Configuration Configuration::FromFile(const std::filesystem::path& filepath) {
  Configuration result;

  std::filesystem::path abs = std::filesystem::canonical(filepath);
  boost::property_tree::read_json(abs.string(), result.mProperties);

  result.setRelativePathTransform(abs.parent_path());

  return result;
}

Configuration Configuration::FromStream(
    std::istream& stream,
    const std::optional<std::filesystem::path>& basePath) {

  Configuration result;

  boost::property_tree::read_json(stream, result.mProperties);

  if (basePath) {
    result.setRelativePathTransform(std::filesystem::absolute(*basePath));
  }

  return result;
}

void Configuration::setRelativePathTransform(const std::filesystem::path& base) {
  this->mTransform.add<std::filesystem::path>(
    [directory = base](std::filesystem::path& path) {
      if (!path.empty() && path.is_relative()) {
        path = std::filesystem::absolute(directory / path);
      }
    }
  );
}


Configuration Configuration::get_child(const boost::property_tree::ptree::path_type& path) const {
  Configuration result;
  result.mProperties = this->mProperties.get_child(path);
  result.mTransform = this->mTransform;
  return result;
}


}
