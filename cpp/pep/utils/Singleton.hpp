#pragma once

#include <cassert>
#include <stdexcept>
#include <type_traits>

namespace pep {

template <class TDerived>
class StaticSingleton;

/// \brief Base class for singleton types: enforces that only a single instance can exist
/// \tparam TDerived the derived type that may only be instantiated once
/// \remark We'd like to ensure that _only_ TDerived can inherit from Singleton<TDerived>, or at least that our
        /// constructor can only be called during construction of a TDerived instance. But we can't make our constructor
        /// "private" and be-"friend TDerived" because it would break situations where TDerived inherits from us through
        /// an intermediate ancestor. And we can't check it at run time using dynamic_cast<TDerived*> during
        /// construction because (from https://en.cppreference.com/w/cpp/language/dynamic_cast) "If target-type is not a
        /// pointer or reference to the constructor's/destructor's own class or one of its bases, the behavior is
        /// undefined.". So instead we require that this class be inherited through a lifetime-managing class that _can_
        /// access our constructor.
template <class TDerived>
class Singleton {
  friend class StaticSingleton<TDerived>;

private:
  using Address = Singleton*;

  // Defining our static variable in a function ensures that it's been initialized when other static variables try to reference it.
  // Read up on "static initialization fiasco" to learn more.
  static Address& InstanceAddress() noexcept { // Note: returns by reference
    static Address result = nullptr;
    return result;
  }

  Singleton() {
    static_assert(std::is_base_of_v<Singleton<TDerived>, TDerived>, "TDerived must inherit from this class");

    if (InstanceAddress() != nullptr) {
      throw std::logic_error("Can't create a second singleton instance");
    }
    InstanceAddress() = this;
  }

public:
  /// \brief Destructor
  ~Singleton() noexcept {
    assert(InstanceAddress() == this);
    InstanceAddress() = nullptr;
  }

  /// \brief No copy construction: we only allow a single instance
  Singleton(const Singleton&) = delete;
  /// \brief No copy assignment: we only allow a single instance
  Singleton& operator=(const Singleton&) = delete;

  /// \brief No move construction: we'd need to perform magic on our static InstanceAddress
  Singleton(Singleton&& other) = delete;
  /// \brief No move assignment: we'd need to perform magic on our static InstanceAddress
  Singleton& operator=(Singleton&& other) = delete;
};


/// \brief Base class for singleton types that (want to) provide a default-constructed instance with static storage duration
/// \tparam TDerived the derived type that may only be instantiated once
/// \remark Don't specialize so that Singleton<> can assume proper derivation/instantiation: see that class's documentation
template <class TDerived>
class StaticSingleton : public Singleton<TDerived> {
protected:
  StaticSingleton() = default;

  // Defining our static variable in a function ensures that it's been initialized when other static variables try to reference it.
  // Read up on "static initialization fiasco" to learn more.
  static TDerived& Instance() {
    static_assert(std::is_base_of_v<StaticSingleton<TDerived>, TDerived>, "TDerived must inherit from this class");

    // From the C++20 standard (https://isocpp.org/files/papers/N4860.pdf), chapter 8.8 "Declaration statement" [stmt.dcl]:
    // "initialization of a block-scope variable with static storage duration [...] is performed the first time control
    // passes through its declaration [...] If control enters the declaration concurrently while the variable is being
    // initialized, the concurrent execution shall wait for completion of the initialization."
    // So we don't have to synchronize for possible multi-threaded callers.
    static TDerived result;
    return result;
  }
};

}
