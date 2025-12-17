#include <functional>
#include <optional>
#include <type_traits>

namespace pep {

template <typename T>
using OptionalRef = std::optional<std::reference_wrapper<T>>;

template <typename T>
OptionalRef<T> OptionalRefFromPtr(T* const t) {
  return (t == nullptr) ? OptionalRef<T>{} : OptionalRef<T>{*t};
}

template <typename T>
OptionalRef<const T> OptionalCRefFromPtr(T* const t) {
  return OptionalRefFromPtr(t);
}

static_assert(
    std::is_same_v<decltype(OptionalRefFromPtr(std::declval<float*>())), OptionalRef<float>>,
    "OptionalRefFromPtr returns a non-const ref if the param type is NOT const");
static_assert(
    std::is_same_v<decltype(OptionalRefFromPtr(std::declval<const float*>())), OptionalRef<const float>>,
    "OptionalRefFromPtr returns a const ref if the param type IS const");
static_assert(
    std::is_same_v<decltype(OptionalCRefFromPtr(std::declval<float*>())), OptionalRef<const float>>,
    "OptionalCRefFromPtr always returns a const ref");

} // namespace pep
