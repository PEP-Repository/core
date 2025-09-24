#pragma once

#include <pep/utils/MultiTypeTransform.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

#include <filesystem>
#include <boost/property_tree/ptree.hpp>

namespace pep {

/*
Callers: use these frontend functions (SerializeProperties<> and DeserializeProperties<>) exclusively.
*/
template <typename TValue>
void SerializeProperties(boost::property_tree::ptree& destination, const TValue& value);

template <typename TValue>
void SerializeProperties(boost::property_tree::ptree& destination, const boost::property_tree::ptree::path_type& path, const TValue& value);

template <typename TValue>
TValue DeserializeProperties(const boost::property_tree::ptree& source, const MultiTypeTransform& transform);

template <typename TValue>
TValue DeserializeProperties(const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform);

template <typename TValue>
TValue& DeserializeProperties(TValue& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform);

template <typename TValue>
TValue& DeserializeProperties(TValue& destination, const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform);


/*
Implementors:
- Specialize class PropertySerializer<> to have property (de)serialization support a new type.
- Have your specialization inherit from either PropertySerializerByValue<> or PropertySerializerByReference<>,
  whichever you find convenient.
- Override abstract methods "read" and "write" appropriately. Use calls to the frontend functions (above)
  to serialize nested data to ptree nodes.
- If your specialization has special handling for absent nodes, override methods "readChild" and "writeChild"
  as well. Refer to the specializations for e.g. std::vector<> and std::optional<> below.
*/

// Utility base for PropertySerializerByValue<> and PropertySerializerByReference<> (below)
template <typename TValue>
class PropertySerializerBase {
public:
  virtual ~PropertySerializerBase() = default;

  virtual void write(boost::property_tree::ptree& destination, const TValue& value) const = 0;

  virtual void writeChild(boost::property_tree::ptree& destination, const boost::property_tree::ptree::path_type& path, const TValue& value) const {
    boost::property_tree::ptree own;
    write(own, value);
    destination.add_child(path, own);
  }
};

// Base class for PropertySerializer<> specializations that want to deserialize to a return value
template <typename TValue>
class PropertySerializerByValue : public PropertySerializerBase<TValue> {
public:
  virtual TValue read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const = 0;

  virtual TValue readChild(const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) const {
    const auto& own = source.get_child(path);
    return read(own, transform);
  }
};

// Base class for PropertySerializer<> specializations that want to deserialize into an updatable reference
template <typename TValue>
class PropertySerializerByReference : public PropertySerializerBase<TValue> {
public:
  virtual void read(TValue& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const = 0;

  virtual void readChild(TValue& destination, const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) const {
    const auto& own = source.get_child(path);
    read(destination, own, transform);
  }
};

// Unspecialized serialization is forwarded to Boost so that we support all TValue types supported by Boost.
template <typename TValue>
class PropertySerializer : public PropertySerializerByValue<TValue> {
public:
  void write(boost::property_tree::ptree& destination, const TValue& value) const override {
    destination.put_value(value);
  }

  TValue read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override {
    return source.get_value<TValue>();
  }
};

// Functions that adapt by-reference deserialization requests to by-value serializers and vice versa
namespace detail {
  template <typename T>
  T DeserializeByValue(const PropertySerializerByValue<T>& serializer, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) {
    auto result = serializer.read(source, transform);
    transform(result);
    return result;
  }

  template <typename T>
  T DeserializeByValue(const PropertySerializerByValue<T>& serializer, const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) {
    auto result = serializer.readChild(source, path, transform);
    transform(result);
    return result;
  }

  template <typename T>
  T DeserializeByValue(const PropertySerializerByReference<T>& serializer, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) {
    T result;
    serializer.read(result, source, transform);
    transform(result);
    return result;
  }

  template <typename T>
  T DeserializeByValue(const PropertySerializerByReference<T>& serializer, const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) {
    T result;
    serializer.readChild(result, source, path, transform);
    transform(result);
    return result;
  }

  template <typename T>
  void DeserializeByReference(const PropertySerializerByValue<T>& serializer, T& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) {
    destination = serializer.read(source, transform);
    transform(destination);
  }

  template <typename T>
  void DeserializeByReference(const PropertySerializerByValue<T>& serializer, T& destination, const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) {
    destination = serializer.readChild(source, path, transform);
    transform(destination);
  }

  template <typename T>
  void DeserializeByReference(const PropertySerializerByReference<T>& serializer, T& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) {
    serializer.read(destination, source, transform);
    transform(destination);
  }

  template <typename T>
  void DeserializeByReference(const PropertySerializerByReference<T>& serializer, T& destination, const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) {
    serializer.readChild(destination, source, path, transform);
    transform(destination);
  }
}

// Implementation of frontend functions
template <typename TValue>
void SerializeProperties(boost::property_tree::ptree& destination, const TValue& value) {
  PropertySerializer<TValue>().write(destination, value);
}

template <typename TValue>
void SerializeProperties(boost::property_tree::ptree& destination, const boost::property_tree::ptree::path_type& path, const TValue& value) {
  PropertySerializer<TValue>().writeChild(destination, path, value);
}

template <typename TValue>
TValue DeserializeProperties(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) {
  return detail::DeserializeByValue(PropertySerializer<TValue>(), source, transform);
}

template <typename TValue>
TValue DeserializeProperties(const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) {
  return detail::DeserializeByValue(PropertySerializer<TValue>(), source, path, transform);
}

template <typename TValue>
TValue& DeserializeProperties(TValue& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) {
  detail::DeserializeByReference(PropertySerializer<TValue>(), destination, source, transform);
  return destination;
}

template <typename TValue>
TValue& DeserializeProperties(TValue& destination, const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) {
  detail::DeserializeByReference(PropertySerializer<TValue>(), destination, source, path, transform);
  return destination;
}

// PropertySerializer<> specializations
template <typename TValue>
class PropertySerializer<std::vector<TValue>> : public PropertySerializerByReference<std::vector<TValue>> {
private:
  using Base = PropertySerializerByReference<std::vector<TValue>>;

public:
  void write(boost::property_tree::ptree& destination, const std::vector<TValue>& value) const override {
    if (value.empty()) {
      throw std::runtime_error("Cannot properly serialize empty vector to property tree");
    }
    for (const auto& entry : value) {
      boost::property_tree::ptree own;
      SerializeProperties(own, entry);
      destination.push_back(std::make_pair("", own));
    }
  }

  void read(std::vector<TValue>& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override {
    if (source.empty()) {
      if (!source.data().empty()) {
        throw std::runtime_error("Cannot read vector data from non-array node");
      }
    }
    else {
      auto end = source.end();
      auto named = std::find_if(source.begin(), end, [](const boost::property_tree::ptree::value_type& entry) {return !entry.first.empty(); });
      if (named != end) {
        throw std::runtime_error("Vector can only be read from node with unnamed entries");
      }
    }
    destination.clear(); // TODO: prevent destination from having been updated if an exception is raised below
    for (const auto& entry : source) {
      destination.push_back(DeserializeProperties<TValue>(entry.second, transform));
    }
  }

  void writeChild(boost::property_tree::ptree& destination, const boost::property_tree::ptree::path_type& path, const std::vector<TValue>& value) const override {
    // Prevent Boost from serializing empty vector as empty string, i.e. "path": ""
    if (!value.empty()) {
      Base::writeChild(destination, path, value);
    }
  }

  void readChild(std::vector<TValue>& destination, const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) const override {
    const auto& own = source.get_child_optional(path);
    if (own) {
      read(destination, *own, transform);
    }
    else {
      destination.clear();
    }
  }
};

template <typename TValue>
class PropertySerializer<std::unordered_map<std::string, TValue>> : public PropertySerializerByReference<std::unordered_map<std::string, TValue>> {
public:
  using Map = std::unordered_map<std::string, TValue>;

private:
  using Base = PropertySerializerByReference<Map>;

public:
  void write(boost::property_tree::ptree& destination, const Map& value) const override {
    for (const auto& entry : value) {
      boost::property_tree::ptree own;
      SerializeProperties(own, entry.second);
      destination.push_back(std::make_pair(entry.first, own));
    }
  }

  void read(Map& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override {
    destination.clear(); // TODO: prevent destination from having been updated if an exception is raised below
    for (const auto& entry : source) {
      auto emplaced = destination.emplace(std::make_pair(entry.first, DeserializeProperties<TValue>(entry.second, transform)));
      if (!emplaced.second) {
        throw std::runtime_error("Cannot add duplicate key '" + entry.first + "' to unordered map");
      }
    }
  }
};

template <typename TValue>
class PropertySerializer<std::optional<TValue>> : public PropertySerializerByReference<std::optional<TValue>> {
private:
  using Base = PropertySerializerByReference<std::optional<TValue>>;

public:
  void write(boost::property_tree::ptree& destination, const std::optional<TValue>& value) const override {
    if (!value) {
      throw std::runtime_error("Cannot write unset optional value to property tree");
    }
    SerializeProperties(destination, *value);
    if (destination.empty() && destination.data().empty()) {
      throw std::runtime_error("Non-empty optional value must produce serialization data");
    }
  }

  void read(std::optional<TValue>& destination, const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override {
    std::optional<std::string> content;
    try {
      content = DeserializeProperties<std::string>(source, MultiTypeTransform()); // Prevent transformation: we want the raw "null" value if it's there
    }
    catch (const boost::property_tree::ptree_error&) {
      // Ignore: Couldn't deserialize as string: this wasn't a JSON null value or "null" string
    }
    if (content) {
      if (*content == "null") {
        throw std::runtime_error("Input not supported: cannot discriminate between JSON \"null\" string and null value");
      }
    }
    destination = DeserializeProperties<TValue>(source, transform);
  }

  void writeChild(boost::property_tree::ptree& destination, const boost::property_tree::ptree::path_type& path, const std::optional<TValue>& value) const override {
    // Unset optional value does not produce a path+value pair
    if (value) {
      Base::writeChild(destination, path, value);
    }
  }

  void readChild(std::optional<TValue>& destination, const boost::property_tree::ptree& source, const boost::property_tree::ptree::path_type& path, const MultiTypeTransform& transform) const override {
    const auto& own = source.get_child_optional(path);
    if (own) {
      read(destination, *own, transform);
    }
    else {
      destination = std::nullopt;
    }
  }
};

template <>
class PropertySerializer<std::filesystem::path> : public PropertySerializerByValue<std::filesystem::path> {
public:
  void write(boost::property_tree::ptree& destination, const std::filesystem::path& value) const override {
    SerializeProperties(destination, value.string());
  }

  std::filesystem::path read(const boost::property_tree::ptree& source, const MultiTypeTransform& transform) const override {
    return DeserializeProperties<std::string>(source, transform);
  }
};

}
