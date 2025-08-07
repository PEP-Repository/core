#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pep::structuredOutput {

/// Maps values of \c T to deduplicated strings and provides stable index-based access to those strings
template <typename T>
class IndexedStringPool final {
public:
  using Projection = std::function<std::string(const T&)>;

  /// @brief Non-owning pointer to a string in a StringPool
  /// @details Remains valid until the StringPool is destructed or reassigned
  class Ptr;

  /// Constructs a pool to map values of \c T to string via \p toString
  /// @pre toString must always return the same output for the same input
  explicit IndexedStringPool(Projection toString) : mToString(std::move(toString)) {}

  /// Maps /p t to a (new or existing) string in the pool
  Ptr map(const T& t);

  /// All strings in the pool in the order they were added
  const std::vector<std::string_view>& all() const noexcept { return mAll; }

private:
  /// Inserts a new element with strong exception safety (commit or rollback)
  Ptr AttemptInsertTransaction(std::string&&);

  Projection mToString;                                   ///< Mapping function
  std::unordered_map<std::string, std::size_t> mIndexMap; ///< The strings together with their index in mAll
  std::vector<std::string_view> mAll;                     ///< All strings in the order that they were added
};

template <typename T>
class IndexedStringPool<T>::Ptr final {
public:
  /// The index of the string within the parent stringpool
  std::size_t index() const noexcept { return mIndex; }
  const std::string_view& operator*() const noexcept { return mData; }
  const std::string_view* operator->() const noexcept { return &this->operator*(); }

private:
  friend class IndexedStringPool<T>;
  Ptr(const std::string& data, std::size_t index) : mData{data.begin(), data.end()}, mIndex{index} {}

  std::string_view mData;
  std::size_t mIndex;
};

template <typename T>
typename IndexedStringPool<T>::Ptr IndexedStringPool<T>::AttemptInsertTransaction(std::string&& str) {
  mAll.reserve(mAll.size() + 1);                                     // may throw, capacity is not observable
  const auto newEntry = mIndexMap.emplace(str, mAll.size()).first;   // may throw
  mAll.emplace_back(newEntry->first.begin(), newEntry->first.end()); // won't throw (no allocations after reserve)
  return {newEntry->first, newEntry->second};
}

template <typename T>
typename IndexedStringPool<T>::Ptr IndexedStringPool<T>::map(const T& t) {
  auto str = mToString(t);
  const auto existingEntry = mIndexMap.find(str);
  return (existingEntry != mIndexMap.end()) ? Ptr{existingEntry->first, existingEntry->second}
                                            : AttemptInsertTransaction(std::move(str));
}

} // namespace pep::structuredOutput
