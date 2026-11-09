#pragma once

#include <concepts>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pep {

/// \brief A synchronous generator coroutine: a lazily evaluated input range (view) of the values that the coroutine
///        co_yields. A simple substitute for C++23's std::generator, which libc++ doesn't provide (yet).
/// \tparam T The (non-reference, non-cv-qualified) type of the values that are produced.
/// \details Usage is (mostly) the same as for std::generator<T>:
///          \code
///            Generator<int> CountTo(int n) {
///              for (int i = 1; i <= n; ++i) {
///                co_yield i;
///              }
///            }
///            for (int i : CountTo(3)) { ... }
///          \endcode
///          Like std::generator<T>, dereferencing an iterator produces a T&& that refers to the co_yielded value, which
///          can thus be moved from. Yielding an rvalue (such as a temporary) doesn't copy it; yielding an lvalue does.
/// \remark Unlike std::generator, there's no support for std::ranges::elements_of. To yield the elements of another
///         range (such as a nested Generator), loop over it: \code for (auto&& x : other) { co_yield std::move(x); } \endcode
/// \remark Exceptions escaping from the coroutine body are (re)thrown to the code iterating over the Generator, i.e.
///         from begin() or from the iterator's operator++.
/// \remark The coroutine body doesn't start running until begin() is invoked, and it only runs up to the next co_yield
///         when the iterator is incremented. Destroying a Generator destroys the coroutine frame (including its locals
///         and copies of the coroutine's arguments), even if the coroutine hasn't run to completion.
/// \remark A Generator can only be iterated once: begin() may be invoked only once. A Generator must be iterated (and
///         destroyed) on a single thread at a time.
template <typename T>
class Generator : public std::ranges::view_interface<Generator<T>> {
  static_assert(std::is_object_v<T> && std::same_as<T, std::remove_cv_t<T>>,
    "Generator value type must be a non-cv-qualified object type");

public:
  // Well-known name
  class promise_type;

private:
  using Handle = std::coroutine_handle<promise_type>;

  Handle handle_;

  explicit Generator(Handle handle) noexcept : handle_(handle) {}

  /// \brief Resumes the coroutine until it yields a (next) value or finishes, rethrowing any exception it raised.
  static void Advance(Handle handle) {
    handle.resume();
    if (auto exception = std::exchange(handle.promise().exception_, nullptr)) {
      std::rethrow_exception(exception);
    }
  }

public:
  class promise_type {
    friend Generator;

    T* value_{nullptr}; // The most recently yielded value
    std::exception_ptr exception_;
    bool started_{false};

    /// \brief Keeps a copy of a yielded lvalue alive in the coroutine frame while the coroutine is suspended.
    class CopyAwaiter {
      T copy_;
      promise_type& promise_;

    public:
      CopyAwaiter(const T& value, promise_type& promise) : copy_(value), promise_(promise) {}

      // Well-known name
      bool await_ready() const noexcept { return false; }
      // Well-known name
      void await_suspend(std::coroutine_handle<>) noexcept { promise_.value_ = std::addressof(copy_); }
      // Well-known name
      void await_resume() const noexcept {}
    };

  public:
    // Well-known name
    Generator get_return_object() noexcept { return Generator(Handle::from_promise(*this)); }
    // Well-known name: don't start running the body until begin() is invoked
    std::suspend_always initial_suspend() const noexcept { return {}; }
    // Well-known name: keep the frame alive so that the iterator can detect completion
    std::suspend_always final_suspend() const noexcept { return {}; }

    // Well-known name. The (temporary) value lives until the coroutine is resumed, so we can just point to it.
    std::suspend_always yield_value(T&& value) noexcept {
      value_ = std::addressof(value);
      return {};
    }
    // Well-known name
    CopyAwaiter yield_value(const T& value) requires std::copy_constructible<T> {
      return CopyAwaiter(value, *this);
    }

    // Well-known name
    void return_void() const noexcept {}
    // Well-known name
    void unhandled_exception() noexcept { exception_ = std::current_exception(); }

    // Well-known name: generators are synchronous, so disallow co_await in the coroutine body
    void await_transform(auto&&) = delete;
  };

  class Iterator {
    friend Generator;

    Handle handle_;

    explicit Iterator(Handle handle) noexcept : handle_(handle) {}

  public:
    using value_type = T;
    using difference_type = std::ptrdiff_t;

    Iterator(Iterator&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
    Iterator& operator=(Iterator&& other) noexcept {
      handle_ = std::exchange(other.handle_, nullptr);
      return *this;
    }

    T&& operator*() const noexcept { return std::move(*handle_.promise().value_); }

    Iterator& operator++() {
      Advance(handle_);
      return *this;
    }
    void operator++(int) { ++*this; }

    friend bool operator==(const Iterator& it, std::default_sentinel_t) noexcept { return it.handle_.done(); }
  };

  Generator(const Generator&) = delete;
  Generator& operator=(const Generator&) = delete;

  Generator(Generator&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}
  Generator& operator=(Generator&& other) noexcept {
    if (this != &other) {
      if (handle_) {
        handle_.destroy();
      }
      handle_ = std::exchange(other.handle_, nullptr);
    }
    return *this;
  }

  ~Generator() noexcept {
    if (handle_) {
      handle_.destroy();
    }
  }

  /// \brief Starts running the coroutine, up to its first co_yield (or its completion).
  /// \remark May only be invoked once.
  Iterator begin() {
    if (!handle_) {
      throw std::logic_error("Cannot iterate over a moved-from Generator");
    }
    auto& promise = handle_.promise();
    if (promise.started_) {
      throw std::logic_error("A Generator can only be iterated once");
    }
    promise.started_ = true;
    Advance(handle_);
    return Iterator(handle_);
  }

  std::default_sentinel_t end() const noexcept { return {}; }
};

}
