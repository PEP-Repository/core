#include <utility>
#include <optional>
#include <functional>

namespace pep {

template <class T> class OptionalRef;

namespace detail {

template <class T>
struct is_optional_impl : std::false_type {};

template <class T>
struct is_optional_impl<std::optional<T>> : std::true_type {};

template <class T>
struct is_optional_impl<OptionalRef<T>> : std::true_type {};

template <class T>
using is_optional = is_optional_impl<std::decay_t<T>>;

} // namespace detail

/// Optional reference, with the same interface and semantics as (C++26) std::optional
/// @note The implementation is based on https://github.com/steve-downey/optional_ref/blob/main/papers/optional_ref.h,
/// which is the reference implementation for C++26 std::optional<T&>.
///
/// We only made minor modifications, so that we can define this as a separate template class,
/// instead of a specialization std::optional<T>.
///
/// The idea is that we use this as a placeholder until we get proper support for optional_refs in the standard library.
template <class T>
class OptionalRef {
  public:
    using value_type = T&;

  private:
    T* value_; // exposition only

  public:
    //  \rSec3[optional.ctor]{Constructors}

    constexpr OptionalRef() noexcept : value_(nullptr) {}

    constexpr OptionalRef(std::nullopt_t) noexcept : value_(nullptr) {}

    constexpr OptionalRef(const OptionalRef& rhs) noexcept = default;
    constexpr OptionalRef(OptionalRef&& rhs) noexcept      = default;

    template <class U = T>
        requires(!detail::is_optional<std::decay_t<U>>::value)
    constexpr explicit(!std::is_convertible_v<U, T>) OptionalRef(U&& u) noexcept
        : value_(std::addressof(u)) {
        static_assert(
            std::is_constructible_v<std::add_lvalue_reference_t<T>, U>,
            "Must be able to bind U to T&");
        static_assert(std::is_lvalue_reference<U>::value,
                      "U must be an lvalue");
    }

    template <class U>
    constexpr explicit(!std::is_convertible_v<U, T>)
        OptionalRef(const OptionalRef<U>& rhs) noexcept
        : OptionalRef(*rhs) {}

    //  \rSec3[optional.dtor]{Destructor}

    constexpr ~OptionalRef() = default;

    // \rSec3[optional.assign]{Assignment}

    constexpr OptionalRef& operator=(std::nullopt_t) noexcept {
        value_ = nullptr;
        return *this;
    }

    constexpr OptionalRef& operator=(const OptionalRef& rhs) noexcept = default;
    constexpr OptionalRef& operator=(OptionalRef&& rhs) noexcept      = default;

    template <class U = T>
        requires(!detail::is_optional<std::decay_t<U>>::value)
    constexpr OptionalRef& operator=(U&& u) {
        static_assert(
            std::is_constructible_v<std::add_lvalue_reference_t<T>, U>,
            "Must be able to bind U to T&");
        static_assert(std::is_lvalue_reference<U>::value,
                      "U must be an lvalue");
        value_ = std::addressof(u);
        return *this;
    }

    template <template<typename> class OptionalT, class U>
        requires (detail::is_optional<OptionalT<U>>::value)
    constexpr OptionalRef& operator=(const OptionalT<U>& rhs) noexcept {
        static_assert(
            std::is_constructible_v<std::add_lvalue_reference_t<T>, U>,
            "Must be able to bind U to T&");
        value_ = std::addressof(rhs.value());
        return *this;
    }

    template <class U>
        requires(!detail::is_optional<std::decay_t<U>>::value)
    constexpr OptionalRef& emplace(U&& u) noexcept {
        return *this = std::forward<U>(u);
    }

    //   \rSec3[optional.swap]{Swap}

    constexpr void swap(OptionalRef& rhs) noexcept {
        std::swap(value_, rhs.value_);
    }

    // \rSec3[optional.observe]{Observers}
    constexpr T* operator->() const noexcept { return value_; }

    constexpr T& operator*() const noexcept { return *value_; }

    constexpr explicit operator bool() const noexcept {
        return value_ != nullptr;
    }
    constexpr bool has_value() const noexcept { return value_ != nullptr; }

    constexpr T& value() const {
        if (has_value())
            return *value_;
        throw std::bad_optional_access();
    }

    template <class U>
    constexpr T value_or(U&& u) const {
        static_assert(std::is_constructible_v<std::add_lvalue_reference_t<T>,
                                              decltype(u)>,
                      "Must be able to bind u to T&");
        return has_value() ? *value_ : std::forward<U>(u);
    }

    //   \rSec3[optional.monadic]{Monadic operations}

    template <class F>
    constexpr auto and_then(F&& f) const {
        using U = std::invoke_result_t<F, T&>;
        static_assert(detail::is_optional<U>::value,
                      "F must return an optional");
        return (has_value()) ? std::invoke(std::forward<F>(f), *value_)
                             : std::remove_cvref_t<U>();
    }

    template <class F>
    constexpr auto
    transform(F&& f) const -> OptionalRef<std::invoke_result_t<F, T&>> {
        using U = std::invoke_result_t<F, T&>;
        return (has_value())
                   ? OptionalRef<U>{std::invoke(std::forward<F>(f), *value_)}
                   : OptionalRef<U>{};
    }

    template <class F>
    constexpr OptionalRef or_else(F&& f) const {
        using U = std::invoke_result_t<F>;
        static_assert(std::is_same_v<std::remove_cvref_t<U>, OptionalRef>);
        return has_value() ? *value_ : std::forward<F>(f)();
    }

    constexpr void reset() noexcept { value_ = nullptr; }
    using iterator       = T*;
    using const_iterator = const T*;

    // [optional.iterators], iterator support
    constexpr T* begin() noexcept {
        if (has_value()) {
            return value_;
        } else {
            return nullptr;
        }
    }
    constexpr const T* begin() const noexcept {
        if (has_value()) {
            return value_;
        } else {
            return nullptr;
        }
    }
    constexpr T* end() noexcept { return begin() + has_value(); }

    constexpr const T* end() const noexcept { return begin() + has_value(); }

    constexpr std::reverse_iterator<T*> rbegin() noexcept {
        return reverse_iterator(end());
    }
    constexpr std::reverse_iterator<const T*> rbegin() const noexcept {
        return reverse_iterator(end());
    }
    constexpr std::reverse_iterator<T*> rend() noexcept {
        return reverse_iterator(begin());
    }
    constexpr std::reverse_iterator<const T*> rend() const noexcept {
        return reverse_iterator(begin());
    }

    constexpr const T* cbegin() const noexcept { return begin(); }
    constexpr const T* cend() const noexcept { return end(); }
    constexpr std::reverse_iterator<const T*> crbegin() const noexcept {
        return rbegin();
    }
    constexpr std::reverse_iterator<const T*> crend() const noexcept {
        return rend();
    }
};

}
