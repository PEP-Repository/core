#pragma once

#include <cassert>
#include <functional>
#include <optional>
#include <type_traits>

namespace pep {

/// A function taking no parameters and returning no value.
/// \remark The \ref PerformAsAction function adapts any function (signature) to a callback of this type
using Action = std::function<void()>;

/// A function that invokes an \ref Action callback
using ActionPerformer = std::function<void(const Action&)>;

namespace detail {

template <typename TResult>
struct ActionResultAdapter {
  template <typename TFunction, typename... TArgs>
  static TResult Perform(const ActionPerformer& performer, TFunction&& function, TArgs&&... args) {
    static_assert(std::is_same_v<TResult, std::invoke_result_t<TFunction, TArgs...>>, "Wrong ActionResultAdapter specialization selected");

    std::optional<TResult> result;
    performer([&]() {
      assert(!result.has_value());
      result.emplace(function(std::forward<TArgs>(args)...));
      });
    assert(result.has_value());
    return *std::move(result);
  }
};

template <typename T>
struct ActionResultAdapter<T&> {
  template <typename TFunction, typename... TArgs>
  static T& Perform(const ActionPerformer& performer, TFunction&& function, TArgs&&... args) {
    static_assert(std::is_same_v<T&, std::invoke_result_t<TFunction, TArgs...>>, "Wrong ActionResultAdapter specialization selected");

    T* address = nullptr;
    performer([&]() {
      assert(address == nullptr);
      address = std::addressof(function(std::forward<TArgs>(args)...));
      });
    assert(address != nullptr);
    return *address;
  }
};

template <>
struct ActionResultAdapter<void> {
  template <typename TFunction, typename... TArgs>
  static void Perform(const ActionPerformer& performer, TFunction&& function, TArgs&&... args) {
    static_assert(std::is_same_v<void, std::invoke_result_t<TFunction, TArgs...>>, "Wrong ActionResultAdapter specialization selected");

    [[maybe_unused]] bool invoked = false;
    performer([&]() {
      assert(!invoked);
      function(std::forward<TArgs>(args)...);
      invoked = true;
      });
    assert(invoked);
  }
};

}

/// \brief Lets non-templated ActionPerformer functions invoke (callback) functions with heterogeneous signatures.
/// \tparam TFunction The type of (callback) function to invoke
/// \tparam TArgs Types of parameters that will be passed to the function
/// \param performer A function that invokes its Action parameter
/// \param function The function to invoke
/// \param args Parameters to pass to the function
/// \return The function's return value
/// \remark Adapts heterogeneous (e.g. templated) function invocations to an Action so that the invocation can be
///         performed by non-templated code (that e.g. performs setup and/or teardown).
/// \code
///   class Database {
///   private:
///     struct Implementor;
///     std::unique_ptr<Implementor> pImpl_;
///
///     void invokeInTransaction(const Action& action); // Defined in .cpp so it can access *pImpl_
/// 
///   public:
///     ~Database(); // Defined in .cpp so it can destroy *pImpl_
/// 
///     template <typename TFunction, typename... TArgs>
///     decltype(auto) transactionally(TFunction&& function, TArgs&&... args) {
///       return PerformAsAction(std::bind(&Database::invokeInTransaction, this, std::placeholders::_1),
///         std::forward<TFunction>(function), std::forward<TArgs>(args)...);
///     }
/// \endcode
template <typename TFunction, typename... TArgs>
decltype(auto) PerformAsAction(const ActionPerformer& performer, TFunction&& function, TArgs&&... args) {
  using Result = std::invoke_result_t<TFunction, TArgs...>;
  return detail::ActionResultAdapter<Result>::Perform(performer, std::forward<TFunction>(function), std::forward<TArgs>(args)...);
}

}
