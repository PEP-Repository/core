#include <pep/utils/OptionalRef.hpp>

namespace pep {

/// Accepts class specializations of `std::variant`
template <typename T>
concept OptionalType = detail::is_optional<T>::value;

template <typename T>
using OptionalCRef = OptionalRef<const T>;

/// Constructs a OptionalRef from a pointer
/// @note Returns a OptionalCRef if T is const
template <typename T>
OptionalRef<T> AsOptionalRef(T* const t) {
  return (t == nullptr) ? OptionalRef<T>{} : OptionalRef<T>{*t};
}

/// Constructs a OptionalCRef from a pointer
/// @note Use this if you always want to return a const ref even if T is not const
template <typename T>
OptionalCRef<T> AsOptionalCRef(T* t) {
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

/// Convenience function aliasing ref.value.get()
/// @throws std::bad_optional_access if \p ref does not contain a value.
/// @note Always returns a mutable ref. Does not work for OptionalCRef inputs
template <typename T>
requires(!std::is_const_v<T>)
T& AsRef(OptionalRef<T>& ref) {
  return ref.value();
}

/// Convenience function aliasing ref.value.get()
/// @throws std::bad_optional_access if \p ref does not contain a value.
template <typename T>
const T& AsCRef(const OptionalRef<T>& ref) {
  return ref.value();
}

/// Convenience function aliasing ref.value.get()
/// @throws std::bad_optional_access if \p ref does not contain a value.
template <typename T>
const T& AsCRef(const OptionalCRef<T>& ref) {
  return ref.value();
}

static_assert(
    std::is_same_v<decltype(AsRef(std::declval<OptionalRef<double>&>())), double&>,
    "AsRef returns a plain ref");
static_assert(
    std::is_same_v<decltype(AsCRef(std::declval<OptionalRef<double>&>())), const double&>,
    "AsCRef returns a const ref (when the input is non-const)");
static_assert(
    std::is_same_v<decltype(AsCRef(std::declval<OptionalCRef<double>&>())), const double&>,
    "AsCRef returns a const ref (when the input is const)");

/// Returns a pointer to the value, if \p ref has a value, and returns `nullptr` if \p ref has no value
template <typename T>
requires(!std::is_const_v<T>)
T* AsPtr(OptionalRef<T>& ref) {
  return (ref.has_value()) ? &ref.value() : nullptr;
}

/// Returns a pointer to the value, if \p ref has a value, and returns `nullptr` if \p ref has no value
template <typename T>
const T* AsPtrToConst(const OptionalRef<T>& ref) {
  return (ref.has_value()) ? &ref.value() : nullptr;
}

/// Returns a pointer to the value, if \p ref has a value, and returns `nullptr` if \p ref has no value
template <typename T>
const T* AsPtrToConst(const OptionalCRef<T>& ref) {
  return (ref.has_value()) ? &ref.value() : nullptr;
}

static_assert(
    std::is_same_v<decltype(AsPtr(std::declval<OptionalRef<double>&>())), double*>,
    "AsPtr returns a plain ptr");
static_assert(
    std::is_same_v<decltype(AsPtrToConst(std::declval<OptionalRef<double>&>())), const double*>,
    "AsPtrToConst returns a ptr to a const value");
static_assert(
    std::is_same_v<decltype(AsPtrToConst(std::declval<OptionalCRef<double>&>())), const double*>,
    "AsPtrToConst returns a ptr to a const value");

} // namespace pep
