#pragma once

#include <cassert>
#include <functional>
#include <optional>
#include <type_traits>

namespace pep {

template <typename TResource>
struct ScopedResourceManagement {
  template <typename T> static constexpr inline bool IsSetupFunction = std::is_invocable_r_v<TResource, T>;
  template <typename T> static constexpr inline bool IsTeardownFunction = std::is_invocable_r_v<void, T, TResource>;
};

template <>
struct ScopedResourceManagement<void> {
  template <typename T> static constexpr inline bool IsSetupFunction = std::is_invocable_r_v<void, T>;
  template <typename T> static constexpr inline bool IsTeardownFunction = std::is_invocable_r_v<void, T>;

};

template <typename T, typename TResource>
concept ResourceSetupFunction = ScopedResourceManagement<TResource>::template IsSetupFunction<T>;

template <typename T, typename TResource>
concept ResourceTeardownFunction = ScopedResourceManagement<TResource>::template IsTeardownFunction<T>;


template <typename TRaw>
class ScopedResource final {
private:
  std::function<void(TRaw)> teardown_;
  TRaw raw_;
  bool owner_ = true;

public:
  template <ResourceSetupFunction<TRaw> TSetup, ResourceTeardownFunction<TRaw> TTeardown>
  ScopedResource(const TSetup& setup, TTeardown teardown)
    : teardown_(std::move(teardown)), raw_(setup()) {
  }

  ScopedResource(ScopedResource&& other) noexcept
    : teardown_(std::move(other.teardown_)), raw_(std::move(other.raw_)) {
    static_assert(std::is_nothrow_move_constructible_v<decltype(teardown_)>);
    static_assert(std::is_nothrow_move_constructible_v<decltype(raw_)>);
    other.owner_ = false;
  }

  ScopedResource(const ScopedResource&) = delete;
  ScopedResource& operator=(const ScopedResource&) = delete;
  ScopedResource& operator=(ScopedResource&& other) = delete;

  ~ScopedResource() noexcept {
    if (owner_) {
      teardown_(std::move(raw_));
    }
  }

  const TRaw& raw() const noexcept { return raw_; }
  decltype(auto) operator*() const noexcept { return this->raw(); }
};

template <>
class ScopedResource<void> final {
private:
  std::optional<std::function<void()>> teardown_;

public:
  template <ResourceSetupFunction<void> TSetup, ResourceTeardownFunction<void> TTeardown>
  explicit ScopedResource(TSetup&& setup, const TTeardown& teardown) {
    setup();
    try {
      teardown_.emplace(teardown);
    } catch (...) {
      // The setup() has completed, but our destructor won't (be called to) perform teardown
      teardown();
      throw;
    }
  }

  template <ResourceTeardownFunction<void> TTeardown>
  explicit ScopedResource(TTeardown&& teardown)
    : teardown_(std::forward<TTeardown>(teardown)) {
  }

  ScopedResource(ScopedResource&& other) noexcept {
    swap(teardown_, other.teardown_);
    assert(!other.teardown_.has_value());
  }

  ~ScopedResource() noexcept {
    if (teardown_.has_value()) {
      (*teardown_)();
    }
  }

  ScopedResource(const ScopedResource&) = delete;
  ScopedResource& operator=(const ScopedResource&) = delete;
  ScopedResource& operator=(ScopedResource&&) = delete;
};


template <typename T>
concept AnyResourceSetupFunction = std::is_invocable_v<T>;

template <AnyResourceSetupFunction TSetup>
using ResourceProducedBy = std::invoke_result_t<TSetup>;

template <typename T, typename TSetup>
concept ResourceTeardownFunctionFor = ResourceTeardownFunction<T, ResourceProducedBy<TSetup>>;

template <AnyResourceSetupFunction TSetup, ResourceTeardownFunctionFor<TSetup> TTeardown>
auto MakeScopedResource(TSetup&& setup, TTeardown&& teardown) {
  return ScopedResource<ResourceProducedBy<TSetup>>(std::forward<decltype(setup)>(setup), std::forward<decltype(teardown)>(teardown));
}

template <ResourceTeardownFunction<void> TTeardown>
auto MakeScopedResource(TTeardown&& teardown) {
  return ScopedResource<void>([]() {}, std::forward<TTeardown>(teardown));
}

}
