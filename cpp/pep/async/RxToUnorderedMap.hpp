#pragma once

#include <unordered_map>
#include <rxcpp/rx-lite.hpp>
#include <rxcpp/operators/rx-reduce.hpp>

namespace pep {

namespace detail {

/// Implementor class for RxToUnorderedMap<> function (below).
template <typename TGetKey>
class RxToUnorderedMapOperator {
private:
  std::shared_ptr<TGetKey> mGetKey;

public:
  template <typename TItem>
  struct KeyFinder {
    using Key = typename std::invoke_result<TGetKey, const TItem&>::type;
  };

  explicit RxToUnorderedMapOperator(const TGetKey& getKey)
    : mGetKey(std::make_shared<TGetKey>(getKey)) {
  }

  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<std::unordered_map<typename KeyFinder<TItem>::Key, TItem>>> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    using Key = typename KeyFinder<TItem>::Key;
    using Map = std::unordered_map<Key, TItem>;
    return items.reduce(
      std::make_shared<Map>(),
      [getKey = mGetKey](std::shared_ptr<Map> map, TItem item) {
      auto key = (*getKey)(std::as_const(item));
      if (!map->emplace(std::move(key), std::move(item)).second) {
        throw std::runtime_error("Could not insert duplicate key into unordered map");
      }
      return map;
    });
  }
};

}

/// \brief Collects the emissions of an observable into (an observable emitting) (a shared pointer to) a single \c std::unordered_map.
/// \code
///   myObservable.op(RxToUnorderedMap([](const TItem& item) {return item.key;}))
/// \endcode
/// \return An object that collects individual emissions into an std::unordered_map.
/// \tparam TGetKey A callable type that accepts a TItem and produces the corresponding key for the unordered_map.
template <typename TGetKey>
detail::RxToUnorderedMapOperator<TGetKey> RxToUnorderedMap(const TGetKey& getKey) {
  return detail::RxToUnorderedMapOperator<TGetKey>(getKey);
}

}
