#pragma once

#include <pep/utils/PropertySerializer.hpp>


namespace pep {

class Configuration {
private:
  boost::property_tree::ptree mProperties;
  MultiTypeTransform mTransform;

  void setRelativePathTransform(const std::filesystem::path& base);

public:

  static Configuration FromFile(const std::filesystem::path& filepath);

  static Configuration FromStream(
          std::istream& stream,
          const std::optional<std::filesystem::path>& basePath = std::nullopt);

  Configuration get_child(
          const boost::property_tree::ptree::path_type& path) const;

  template <typename T>
  T get(const boost::property_tree::ptree::path_type& path) const {
      return DeserializeProperties<T>(mProperties, path, mTransform);
  }

  template <typename T> // TODO: remove; have callers use get<std::optional<T>> instead
  T get(const boost::property_tree::ptree::path_type& path, const T& defaultValue) const {
    auto configured = DeserializeProperties<std::optional<T>>(mProperties, path, mTransform);
    if (configured) {
      return *configured;
    }
    T result = defaultValue;
    mTransform(result);
    return result;
  }
};


}
