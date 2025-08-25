#pragma once

#include <pep/utils/VectorOfVectors.hpp>

#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <rxcpp/operators/rx-take.hpp> // Should be included before rx-group_by.hpp: see e.g. https://gitlab.pep.cs.ru.nl/pep/core/-/jobs/241940#L13
#include <rxcpp/operators/rx-group_by.hpp>

#include <rxcpp/operators/rx-ignore_elements.hpp>
#include <rxcpp/operators/rx-reduce.hpp>
#include <rxcpp/operators/rx-tap.hpp>

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
      [getKey = mGetKey](std::shared_ptr<Map> map, const TItem& item) {
      if (!map->emplace((*getKey)(item), item).second) {
        throw std::runtime_error("Could not insert duplicate key into unordered map");
      }
      return map;
    });
  }
};

/// Implementor class for RxBeforeTermination<> function (below).
class RxBeforeTerminationOperator {
public:
  using Handler = std::function<void(std::optional<std::exception_ptr>)>;
private:
  Handler mHandle;

public:
  explicit RxBeforeTerminationOperator(const Handler& handle)
    : mHandle(handle) {
  }

  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.tap(
      [](const TItem&) {/*ignore*/},
      [handle = mHandle](std::exception_ptr ep) {handle(ep); },
      [handle = mHandle]() {handle(std::nullopt); }
    );
  }
};

/// Implementor class for RxInstead<> function (below).
template <typename T>
struct RxInsteadOperator {
private:
  T mReplacement;

public:
  explicit RxInsteadOperator(const T& replacement)
    : mReplacement(replacement) {
  }

  template <typename TItem, typename SourceOperator>
  rxcpp::observable<T> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    static_assert(!rxcpp::is_observable<TItem>::value,
                  "RxInstead used on observable<observable<T>>, which would not wait for the inner observables; "
                  "you probably forgot a flat_map");
    return items
      .ignore_elements()
      .reduce(
        mReplacement,
        [](const T& replacement, const TItem&) {assert(false); return  replacement; } // Should never be called due to .ignore_elements() above
      );
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

/// \brief Aggregates the emissions of an observable into (an observable emitting) (a shared pointer to) a single vector.
/// \code
///   myObservable.op(RxToVector())
/// \endcode
struct RxToVector {
  /// \param items The observable emitting individual items.
  /// \return An observable emitting a single shared_ptr<vector<TItem>>.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<std::vector<TItem>>> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.reduce(
      std::make_shared<std::vector<TItem>>(),
      [](std::shared_ptr<std::vector<TItem>> result, const TItem& item) {
      result->push_back(item);
      return result;
    });
  }
};

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

/// \brief Aggregates the emissions of an observable into (an observable emitting) (a shared pointer to) a single \c std::set<>.
/// \code
///   myObservable.op(RxToSet()).
/// \endcode
struct RxToSet {
private:
  bool mThrowOnDuplicate;

public:
  explicit inline RxToSet(bool throwOnDuplicate = true)
    : mThrowOnDuplicate(throwOnDuplicate) {
  }

  /// \param items The observable emitting individual items.
  /// \return An observable emitting a single shared_ptr<set<TItem>>.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<std::set<TItem>>> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.reduce(
      std::make_shared<std::set<TItem>>(),
      [throwOnDuplicate = mThrowOnDuplicate](std::shared_ptr<std::set<TItem>> set, const TItem& item) {
        auto added = set->emplace(item).second;
        if (throwOnDuplicate && !added) {
          throw std::runtime_error("Could not insert duplicate item into set");
        }
        return set;
      });
  }
};

/// \brief For a source observable that emits vector<TItem> values, aggregates the items into (an observable emitting) (a shared pointer to) a single vector.
/// \remark If you just need items to be aggregated into a single container (and not into contiguous memory), consider using the more efficient RxToVectorOfVectors instead.
/// \code
///   myObservable.op(RxConcatenateVectors()).
/// \endcode
struct RxConcatenateVectors {
  /// \param chunks The observable emitting vectors containing individual items.
  /// \return An observable emitting a single shared_ptr<vector<TItem>>.
  /// \tparam TItem The type of item stored in the vector<> produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<std::vector<TItem>>> operator()(rxcpp::observable<std::vector<TItem>, SourceOperator> chunks) const {
    return chunks.reduce(
      std::make_shared<std::vector<TItem>>(),
      [](std::shared_ptr<std::vector<TItem>> result, const std::vector<TItem>& chunk) {
      result->reserve(result->size() + chunk.size());
      result->insert(result->end(), chunk.cbegin(), chunk.cend());
      return result;
    });
  }
};

/// \brief Aggregates the vector<TItem> emissions of an observable into (an observable emitting) (a shared pointer to) a single VectorOfVectors<TItem>.
/// \code
///   myObservable.op(RxToVectorOfVectors())
/// \endcode
struct RxToVectorOfVectors {
  /// \param items The observable emitting individual vector<TItem> instances.
  /// \return An observable emitting a single shared_ptr<VectorOfVectors<TItem>>.
  /// \tparam TItem The type of item stored in the observable's vector instances.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<VectorOfVectors<TItem>>> operator()(rxcpp::observable<std::vector<TItem>, SourceOperator> items) const {
    return items.reduce(
      std::make_shared<VectorOfVectors<TItem>>(),
      [](std::shared_ptr<VectorOfVectors<TItem>> result, std::vector<TItem> single) {
        *result += std::move(single);
        return result;
      });
  }
};

/// Concatenates strings
struct RxConcatenateStrings {
  /// \return An observable emitting a single string
  template <typename SourceOperator>
  auto operator()(rxcpp::observable<std::string, SourceOperator> items) const {
    return items.reduce(
      std::make_shared<std::ostringstream>(), // Needs shared for now because otherwise rxcpp will make copies...
      [](std::shared_ptr<std::ostringstream> result, const std::string& item) {
      *result << item;
      return result;
    }, [](const std::shared_ptr<std::ostringstream>& result) {
      return std::move(*result).str();
    });
  }
};

/// Add indices to each item using std::pair, starting at 0
template <typename TIndex = size_t>
struct RxIndexed {
  template <typename TItem, typename SourceOperator>
  auto operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    // Not thread-safe
    auto index = std::make_shared<TIndex>();
    return items.map([index](TItem item) { return std::pair<TIndex, TItem>{(*index)++, std::move(item)}; });
  }
};

/*! \brief Invokes a callback when an observable has finished emitting items: either because it's done, or because an error occurred.
 * \tparam THandle The callback type, which must be convertible to a function<> accepting an std::optional<std::exception_ptr> parameter and returning void.
 */
template <typename THandle>
detail::RxBeforeTerminationOperator RxBeforeTermination(const THandle& handle) {
  return detail::RxBeforeTerminationOperator(detail::RxBeforeTerminationOperator::Handler(handle)); // Conversion to Handler ensures our handler has the correct signature
}

/// \brief Invokes a callback when an observable has successfully finished emitting items.
struct RxBeforeCompletion {
public:
  using Handler = std::function<void()>;

private:
  Handler mHandler;

public:
  explicit RxBeforeCompletion(Handler handler)
    : mHandler(std::move(handler)) {
  }

  /// \param items The source observable.
  /// \return An observable emitting the same items as the source observable.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    return items.op(RxBeforeTermination([handler = mHandler](std::optional<std::exception_ptr> error) {
      if (!error.has_value()) {
        handler();
      }
      }));
  }
};

/// Makes sure you get one and only one item back from an RX call.
struct RxGetOne {
  std::string errorText;

  /// \param items The observable emitting the item.
  /// \return An observable emitting a single TItem.
  /// \tparam TItem The type of item produced by the observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items.reduce(
      std::optional<TItem>(),
      [errorText = this->errorText](std::optional<TItem> seed, TItem next) {
        if(!seed) {
          return next;
        }
        else {
          throw std::runtime_error("Encountered multiple "+ errorText);
        }
      },
      [errorText = this->errorText](std::optional<TItem> res) {
        if(res) {
          return *res;
        }
        throw std::runtime_error("Encountered no "+ errorText);
      });
  }

  /// \param errorText Custom text to be displayed in the errors when no of multiple items are found.
  RxGetOne(std::string errorText) : errorText(std::move(errorText)) {}
};

/// Converts shared_ptr instances emitted by an observable to shared_ptr instances of another type: \c myObs.Op(RxSharedPtrCast<RequiredType>()).
/// The source item type must be reference convertible to the destination item type.
/// \tparam TDestination The desired (destination) item type.
template <typename TDestination>
struct RxSharedPtrCast {
  /// \param items The observable emitting the item.
  /// \return An observable emitting shared_ptr instances to the destination type.
  /// \tparam TItem The item type emitted by the source observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<std::shared_ptr<TDestination>> operator()(rxcpp::observable<std::shared_ptr<TItem>, SourceOperator> items) const {
    static_assert(std::is_convertible<TItem&, TDestination&>::value, "The item type of the source observable must be reference convertible to the destination item type");
    return items.map([](std::shared_ptr<TItem> item) -> std::shared_ptr<TDestination> {return item; });
  }
};

/// \brief Provides an observable's number of items to a callback function.
/// \code
///   myObs.op(RxProvideCount([](size_t size) { std::cout << size << " items"; }))
/// \endcode
/// If the source observable emits an error, the callback is not invoked.
struct RxProvideCount {
  using Handler = std::function<void(size_t)>;

private:
  Handler mHandler;

public:
  template <typename THandler>
  explicit inline RxProvideCount(const THandler& handler)
    : mHandler(handler) {
  }

  /// \param items The observable emitting the items.
  /// \return An observable emitting the source observable's items.
  /// \tparam TItem The item type emitted by the source observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    auto count = std::make_shared<size_t>(0U);
    return items
      .tap(
        [count](const TItem&) {++*count; },
        [](std::exception_ptr) { /* Don't invoke handler, e.g. making it think that the source (successfully) emitted no items */ },
        [count, handler = mHandler] {handler(*count); }
      );
  }
};

/// \brief Verifies that an observable emits at least one item.
/// \code
///   myObs.op(RxRequireNonEmpty())
/// \endcode
struct RxRequireNonEmpty : public RxProvideCount {
public:
  /// \param assertOnly Whether to validate the source observable using an assertion (true) or a run time check (false).
  explicit RxRequireNonEmpty(bool assertOnly = false);
};

/*! \brief Exhausts a source observable, then emits a single (specified) item.
/// \code
///   myObs.op(RxInstead(justThisItem))
/// \endcode
 * \param item The item to emit instead of the source observable's items.
 * \remark Mainly intended to help with collections that cannot (easily) be constructed by means of .reduce.
 */
template <typename T>
detail::RxInsteadOperator<T> RxInstead(const T& item) {
  return detail::RxInsteadOperator<T>(item);
}

/// \brief Removes duplicate items from an observable.
/// \code
///   myObs.op(RxDistinct())
/// \endcode
/// \remark Replacement for rxcpp's own \c .distinct() method, which supports disappointingly few item types.
struct RxDistinct {
  /// \param items The observable emitting the items.
  /// \return An observable emitting only unique items, as determined by std::less<TItem>.
  /// \tparam TItem The item type emitted by the source observable.
  /// \tparam SourceOperator The source operator type included in the observable type.
  template <typename TItem, typename SourceOperator>
  rxcpp::observable<TItem> operator()(rxcpp::observable<TItem, SourceOperator> items) const {
    return items
      .op(RxToSet(false))
      .flat_map([](std::shared_ptr<std::set<TItem>> set) {return rxcpp::observable<>::iterate(*set); });
  }
};

}
