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
  explicit IndexedStringPool(Projection toString) : toString_(std::move(toString)) {}

  /// Maps /p t to a (new or existing) string in the pool
  Ptr map(const T& t);

  /// All strings in the pool in the order they were added
  const std::vector<std::string_view>& all() const noexcept { return all_; }

private:
  /// Inserts a new element with strong exception safety (commit or rollback)
  Ptr AttemptInsertTransaction(std::string&&);

  Projection toString_;                                   ///< Mapping function
  std::unordered_map<std::string, std::size_t> indexMap_; ///< The strings together with their index in all_
  std::vector<std::string_view> all_;                     ///< All strings in the order that they were added
};

template <typename T>
class IndexedStringPool<T>::Ptr final {
public:
  /// The index of the string within the parent stringpool
  std::size_t index() const noexcept { return index_; }
  const std::string_view& operator*() const noexcept { return data_; }
  const std::string_view* operator->() const noexcept { return &this->operator*(); }

private:
  friend class IndexedStringPool<T>;
  Ptr(const std::string& data, std::size_t index) : data_{data.begin(), data.end()}, index_{index} {}

  std::string_view data_;
  std::size_t index_;
};

template <typename T>
typename IndexedStringPool<T>::Ptr IndexedStringPool<T>::AttemptInsertTransaction(std::string&& str) {
  all_.reserve(all_.size() + 1);                                     // may throw, capacity is not observable
  const auto newEntry = indexMap_.emplace(str, all_.size()).first;   // may throw
  all_.emplace_back(newEntry->first.begin(), newEntry->first.end()); // won't throw (no allocations after reserve)
  return {newEntry->first, newEntry->second};
}

template <typename T>
typename IndexedStringPool<T>::Ptr IndexedStringPool<T>::map(const T& t) {
  auto str = toString_(t);
  const auto existingEntry = indexMap_.find(str);
  return (existingEntry != indexMap_.end()) ? Ptr{existingEntry->first, existingEntry->second}
                                            : AttemptInsertTransaction(std::move(str));
}

} // namespace pep::structuredOutput
