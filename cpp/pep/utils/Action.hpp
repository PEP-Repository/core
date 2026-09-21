#pragma once

#include <cassert>
#include <functional>
#include <optional>
#include <type_traits>

namespace pep {

/// A function taking no parameters and returning no value.
/// \remark The \ref PerformAsAction function adapts any function (signature) to a callback of this type
using Action = std::function<void()>;

namespace detail {

using ActionPerformer = std::function<void(Action)>;

/// \brief Adapts a function (and its arguments) so it can be passed to an \ref ActionPerformer
/// \tparam TResult The return type of the function
/// \remark This (primary) specialization handles (functions with) non-reference, non-void return types.
template <typename TResult>
struct ActionResultAdapter {
  template <typename TFunction, typename... TArgs>
  static TResult Perform(ActionPerformer performer, TFunction&& function, TArgs&&... args) {
    static_assert(std::is_same_v<TResult, std::invoke_result_t<TFunction, TArgs...>>, "Wrong ActionResultAdapter specialization selected");

    // Provide storage space for the function's return value
    std::optional<TResult> result;
    performer([&]() {
      assert(!result.has_value());
      // Invoke the function and store the return value
      result.emplace(function(std::forward<TArgs>(args)...));
      });

    // Return the value that was stored when the function was invoked
    assert(result.has_value());
    return *std::move(result);
  }
};

/// \brief Adapts a function (and its arguments) so it can be passed to an \ref ActionPerformer
/// \tparam TResult The type of reference returned by the function
/// \remark This specialization handles (functions with) reference return types.
template <typename T>
struct ActionResultAdapter<T&> {
  template <typename TFunction, typename... TArgs>
  static T& Perform(ActionPerformer performer, TFunction&& function, TArgs&&... args) {
    static_assert(std::is_same_v<T&, std::invoke_result_t<TFunction, TArgs...>>, "Wrong ActionResultAdapter specialization selected");

    // Provide storage space for (the address of) the reference returned by the function
    T* address = nullptr;
    performer([&]() {
      assert(address == nullptr);
      // Invoke the function and store (a pointer to) the reference that it returns
      address = std::addressof(function(std::forward<TArgs>(args)...));
      });

    // (Dereference the pointer so that we) return the reference that the function returned
    assert(address != nullptr);
    return *address;
  }
};

/// \brief Adapts a function (and its arguments) so it can be passed to an \ref ActionPerformer
/// \remark This specialization handles (functions with) the void return type.
template <>
struct ActionResultAdapter<void> {
  template <typename TFunction, typename... TArgs>
  static void Perform(ActionPerformer performer, TFunction&& function, TArgs&&... args) {
    static_assert(std::is_same_v<void, std::invoke_result_t<TFunction, TArgs...>>, "Wrong ActionResultAdapter specialization selected");

    // Degenerate case (compared to the specializations above): we don't have to store a return value
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
///       return PerformAsAction(std::bind_front(&Database::invokeInTransaction, this),
///         std::forward<TFunction>(function), std::forward<TArgs>(args)...);
///     }
/// \endcode
template <typename TPerformer, typename TFunction, typename... TArgs>
decltype(auto) PerformAsAction(TPerformer&& performer, TFunction&& function, TArgs&&... args) {
  using Result = std::invoke_result_t<TFunction, TArgs...>;
  return detail::ActionResultAdapter<Result>::Perform(detail::ActionPerformer(std::forward<TPerformer>(performer)), std::forward<TFunction>(function), std::forward<TArgs>(args)...);
}

}
