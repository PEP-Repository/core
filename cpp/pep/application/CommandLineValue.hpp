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
  Implementor mItems;

public:
  using iterator = Implementor::iterator;
  using const_iterator = Implementor::const_iterator;

  inline iterator begin() { return mItems.begin(); }
  inline iterator end() { return mItems.end(); }
  inline const_iterator begin() const { return mItems.begin(); }
  inline const_iterator end() const { return mItems.end(); }
  inline const_iterator cbegin() const { return mItems.cbegin(); }
  inline const_iterator cend() const { return mItems.cend(); }

  template <typename T> void add(const T& value) { mItems.push_back(value); }

  inline bool empty() const noexcept { return mItems.empty(); }
  inline size_t count() const noexcept { return mItems.size(); }
};

class NamedValues {
private:
  std::unordered_map<std::string, Values> mEntries;

public:
  inline Values& at(const std::string& key) { return mEntries.at(key); }
  inline const Values& at(const std::string& key) const { return mEntries.at(key); }

  inline auto begin() const { return mEntries.cbegin(); }
  inline auto end() const { return mEntries.cend(); }
  inline auto find(const std::string& key) const { return mEntries.find(key); }
  inline void set(const std::string& key, const Values& values) { mEntries.insert_or_assign(key, values); }

  template <typename T>
  T get(const std::string& key) const;

  template <class T>
  std::optional<T> getOptional(const std::string& key) const;

  template <typename T>
  std::vector<T> getMultiple(const std::string& key) const;

  template <typename T>
  std::vector<T> getOptionalMultiple(const std::string& key) const;

  size_t count(const std::string& key) const noexcept;
  inline bool has(const std::string& key) const noexcept { return mEntries.count(key) != 0U; }
  bool hasAnyOf(std::initializer_list<std::string> key) const noexcept;
  inline size_t erase(const std::string& key) { return mEntries.erase(key); }
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
  auto position = mEntries.find(key);
  if (position != mEntries.cend()) {
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
