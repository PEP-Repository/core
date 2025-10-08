#pragma once

#include <pep/async/RxToUnorderedMap.hpp>
#include <pep/async/RxToVector.hpp>
#include <cassert>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-take.hpp> // Should be included before rx-group_by.hpp: see e.g. https://gitlab.pep.cs.ru.nl/pep/core/-/jobs/241940#L13
#include <rxcpp/operators/rx-group_by.hpp>
#include <rxcpp/operators/rx-map.hpp>

namespace pep {

/// \brief Aggregates the emissions of an observable into (an observable emitting) (a shared pointer to)
/// a single unordered_map containing (shared_ptr to) vectors.
/// \code{.cpp}
///   myObservable.op(RxGroupToVectors([](const TItem& item) { return item.key; }))
/// \endcode
/// \tparam TGetKey A callable type that accepts a TItem and produces the corresponding key for the unordered_map.
/// \remark Use as a replacement for RX's \c group_by operator if you cannot process the groups immediately,
/// since \c grouped_observable<> instances apparently lose their items during copy construction:
/// \code
///   myObservable
///     .group_by([](TItem item) {return item.key;})
///     .flat_map([](rxcpp::grouped_observable<TKey, TItem> group) {
///     return GetSomeKeyDependentValueObservable(group.get_key())
///       .flat_map([group](KeyDependentValue kdv) { // The lambda capture copy constructs the "group"
///         return group.map([kdv](TItem item) {
///           return ProcessItemAndKeyDependentValue(item, kdv); // Never executed because the captured "group" is empty
///         });
///       });
///     });
/// \endcode
template <typename TGetKey>
struct RxGroupToVectors {
private:
  template <typename TItem>
  using KeyFinder = typename detail::RxToUnorderedMapOperator<TGetKey>::template KeyFinder<TItem>;

  TGetKey mGetKey;

public:
  explicit RxGroupToVectors(const TGetKey& getKey)
    : mGetKey(getKey) {
  }

  /// \param items The observable emitting individual items.
  /// \return An observable emitting a single shared_ptr<std::unordered_map<key, std::shared_ptr<vector<TItem>>>>.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<std::unordered_map<typename KeyFinder<TItem>::Key, std::shared_ptr<std::vector<TItem>>>>> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    using Key = typename KeyFinder<TItem>::Key;
    using ItemVector = std::vector<TItem>;
    using Map = std::unordered_map<Key, std::shared_ptr<ItemVector>>;

    return items.group_by(mGetKey)
      .flat_map([](rxcpp::grouped_observable<Key, TItem> group) {
      return group.op(RxToVector())
        .map([key = group.get_key()](std::shared_ptr<ItemVector> items) {
        assert(!items->empty());
        return std::make_pair(key, items);
        });
      })
      .reduce(
        std::make_shared<Map>(),
        [](std::shared_ptr<Map> result, std::pair<Key, std::shared_ptr<ItemVector>> pair) {
          [[maybe_unused]] auto added = result->emplace(pair).second;
          assert(added);
          return result;
        }
      );
  }
};

}
