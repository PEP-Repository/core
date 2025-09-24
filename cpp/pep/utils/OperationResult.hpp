#pragma once

#include <cassert>
#include <exception>
#include <functional>
#include <variant>
#include <boost/system/system_error.hpp>

namespace pep {

/*!
 * \brief Encapsulates the result of an operation (invocation): either its return value, or the exception(_ptr) that occurred.
 * \tparam TValue The return type of the operation.
 */
template <typename TValue>
class OperationResult {
private:
  static_assert(!std::is_same_v<TValue, std::exception_ptr>, "Can't distinguish success value from failure");

  std::variant<TValue, std::exception_ptr> mState;

  explicit OperationResult(TValue&& value)
    : mState(std::move(value)) {
    assert(this->successful());
    assert(this->exception() == nullptr);
  }

  explicit OperationResult(std::exception_ptr exception)
    : mState(std::move(exception)) {
    assert(!this->successful());
    assert(this->exception() != nullptr);
  }

public:
  /*!
   * \brief Returns the exception(_ptr) that occurred during an operation, or nullptr if no exception occurred.
   * \return An exception_ptr that may be nullptr.
   */
  std::exception_ptr exception() const {
    const std::exception_ptr* address = std::get_if<std::exception_ptr>(&mState);
    if (address != nullptr) {
      return *address;
    }
    return std::exception_ptr();
  }

  /*!
   * \brief Produces the operation's return value, or re-raises the exception if one occurred during the operation.
   * \return (A reference to this object's copy of) the operation's return value.
   */
  const TValue& operator*() const {
    auto exception = this->exception();
    if (exception != nullptr) {
      std::rethrow_exception(exception);
    }
    return std::get<TValue>(mState);
  }

  /*!
   * \brief Accesses member(s) of the operation's return value, or re-raises an exception if one occurred during the operation.
   * \return A pointer to (this object's copy of) the operation's return value.
   */
  const TValue* operator->() const {
    return &**this;
  }

  /*!
   * \brief Determines if the operation completed successfully or not.
   * \return TRUE if the operation completed successfully; false if not.
   */
  bool successful() const noexcept {
    return std::holds_alternative<TValue>(mState);
  }

  /*!
   * \brief Determines if the operation completed successfully or not.
   * \return TRUE if the operation completed successfully; false if not.
   */
  explicit operator bool() const noexcept {
    return this->successful();
  }

  /*!
   * \brief Constructs an instance representing the result of a successfully completed operation.
   * \tparam TArgs Argument types to pass to the operation's result value's constructor.
   * \param args Arguments to pass to the operation's result value's constructor.
   * \return An OperationResult representing the result of a successfully completed operation.
   */
  template <typename... TArgs>
  static OperationResult Success(TArgs&&... args) {
    return OperationResult(TValue(std::forward<TArgs>(args)...));
  }

  /*!
   * \brief Constructs an instance representing the result of an operation that failed due to an exception.
   * \param exception The exception that caused the operation not to complete successfully.
   * \return An OperationResult representing the result of an operation that failed due to an exception.
   */
  static OperationResult Failure(std::exception_ptr exception) {
    return OperationResult(exception);
  }
};

/*!
 * \brief Constructs an OperationResult<> instance representing the result of an operation that failed for a reason indicated by a Boost error code.
 * \tparam TValue The type of value that the operation would have produced if successful.
 * \param ec A Boost error code representing the success or (reason for) failure of the operation.
 * \return An OperationResult<> representing failure for the reason indicated by the specified error code.
 * \remark The OperationResult<> will encapsulate a boost::system::system_error for the specified error code.
 */
template <typename TValue>
OperationResult<TValue> BoostOperationResult(const boost::system::error_code& ec) {
  return OperationResult<TValue>::Failure(std::make_exception_ptr(boost::system::system_error(ec)));
}

/*!
 * \brief Constructs an OperationResult<> instance representing the result of an operation that produced a Boost error code and a value.
 * \tparam TValue The type of the produced value.
 * \param ec A Boost error code representing the success or (reason for) failure of the operation.
 * \param value The value produced by the operation.
 * \return An OperationResult<> representing the result of the Boost operation.
 * \remark If the code indicates success, the OperationResult<> will dereference to the specified value.
 *         If the code indicates failure, the OperationResult<> will encapsulate a boost::system::system_error for the specified error code.
 */
template <typename TValue>
OperationResult<TValue> BoostOperationResult(const boost::system::error_code& ec, TValue value) {
  return ec ? BoostOperationResult<TValue>(ec) : OperationResult<TValue>::Success(value);
}


/*!
 * \brief Helper class for operation result notification.
 * \tparam TValue The type of value returned by a successful operation.
 * \remark Provides nested types for invocation attempt notifications and the handling thereof.
 */
template <typename TValue>
struct OperationInvocation {
  /*!
   * \brief The OperationResult<> specialization representing the (successful or failed) result of performing the operation.
   */
  using Result = OperationResult<TValue>;

  /*!
   * \brief A type that can be called with (a const reference to) an operation result object.
   */
  using Handler = std::function<void(const Result&)>;
};

}
