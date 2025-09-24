#pragma once

#include <any>
#include <stdexcept>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace pep {
namespace commandline {

using ProvidedValue = std::optional<std::string>;
using ProvidedValues = std::vector<ProvidedValue>;
using LexedValues = std::unordered_map<std::string, ProvidedValues>;

class Values {
private:
  using Implementor = std::vector<std::any>;
  Implementor mImplementor;

public:
  using iterator = Implementor::iterator;
  using const_iterator = Implementor::const_iterator;

  inline iterator begin() { return mImplementor.begin(); }
  inline iterator end() { return mImplementor.end(); }
  inline const_iterator begin() const { return mImplementor.begin(); }
  inline const_iterator end() const { return mImplementor.end(); }
  inline const_iterator cbegin() const { return mImplementor.cbegin(); }
  inline const_iterator cend() const { return mImplementor.cend(); }

  template <typename T> void add(const T& value) { mImplementor.push_back(value); }

  inline bool empty() const noexcept { return mImplementor.empty(); }
  inline size_t count() const noexcept { return mImplementor.size(); }
};

class NamedValues {
private:
  std::unordered_map<std::string, Values> mImplementor;

public:
  inline Values& operator[](const std::string& key) { return mImplementor[key]; }

  template <typename T>
  void add(const std::string& key, const T& value) { (*this)[key].add(value); }

  void add(const std::string& key, const Values& values);

  template <typename T>
  T get(const std::string& key) const;

  template <class T>
  std::optional<T> getOptional(const std::string& key) const;

  template <typename T>
  std::vector<T> getMultiple(const std::string& key) const;

  template <typename T>
  std::vector<T> getOptionalMultiple(const std::string& key) const;

  size_t count(const std::string& key) const noexcept;
  inline bool has(const std::string& key) const noexcept { return mImplementor.count(key) != 0U; }
  bool hasAnyOf(std::initializer_list<std::string> key) const noexcept;
};

template <typename T>
T NamedValues::get(const std::string& key) const {
  auto all = this->getMultiple<T>(key);
  if (all.size() != 1U) {
    throw std::runtime_error("Cannot retrieve command line value '" + key + "' because there are " + std::to_string(all.size()));
  }
  return all.front();
}

template <typename T>
std::optional<T> NamedValues::getOptional(const std::string& key) const {
  auto all = this->getOptionalMultiple<T>(key);
  if (all.size() > 1U) {
    throw std::runtime_error("Cannot retrieve command line value '" + key + "' because there are " + std::to_string(all.size()));
  }
  if (all.size() == 0) {
    return std::nullopt;
  }
  return all.front();
}

template <typename T>
std::vector<T> NamedValues::getMultiple(const std::string& key) const {
  if (!this->has(key)) {
    throw std::runtime_error("No command line values found for '" + key + "'");
  }
  return this->getOptionalMultiple<T>(key);
}

template <typename T>
std::vector<T> NamedValues::getOptionalMultiple(const std::string& key) const {
  std::vector<T> result;
  auto position = mImplementor.find(key);
  if (position != mImplementor.cend()) {
    const auto& untyped = position->second;
    result.reserve(untyped.count());
    for (const auto& item : untyped) {
      result.push_back(std::any_cast<T>(item));
    }
  }
  return result;
}

}
}
