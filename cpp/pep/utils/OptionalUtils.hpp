#include <functional>
#include <optional>
#include <type_traits>

namespace pep {

template <typename T>
using OptionalRef = std::optional<std::reference_wrapper<T>>;

template <typename T>
using OptionalCRef = OptionalRef<const T>;

template <typename T>
OptionalRef<T> AsOptionalRef(T* const t) {
  return (t == nullptr) ? OptionalRef<T>{} : OptionalRef<T>{*t};
}

template <typename T>
OptionalCRef<T> AsOptionalCRef(T* const t) {
  return AsOptionalRef(t);
}

static_assert(
    std::is_same_v<decltype(AsOptionalRef(std::declval<float*>())), OptionalRef<float>>,
    "OptionalRefFromPtr returns a non-const ref if the param type is NOT const");
static_assert(
    std::is_same_v<decltype(AsOptionalRef(std::declval<const float*>())), OptionalRef<const float>>,
    "OptionalRefFromPtr returns a const ref if the param type IS const");
static_assert(
    std::is_same_v<decltype(AsOptionalCRef(std::declval<float*>())), OptionalRef<const float>>,
    "OptionalCRefFromPtr always returns a const ref");

} // namespace pep
