#pragma once

#include <cassert>
#include <functional>
#include <optional>
#include <vector>
#include <any>
#include <stdexcept>
#include <algorithm>

namespace pep {

class MultiTypeTransform
{
private:
  std::vector<std::any> mCallbacks; // Contains (specializations of) std::function<void(T&)>

private:
  template <typename T>
  std::optional<std::function<void(T&)>> tryGetCallback() const {
    using Stored = const std::function<void(T&)>;
    Stored* found = nullptr;
    [[maybe_unused]] auto position = std::find_if(mCallbacks.cbegin(), mCallbacks.cend(), [&found](const std::any& candidate) {
      assert(found == nullptr);
      // We update the result from here so that the caller doesn't have to invoke std::any_cast<> again
      found = std::any_cast<Stored>(&candidate);
      return found != nullptr;
    });
    assert((found == nullptr) == (position == mCallbacks.cend()));
    if (found != nullptr) {
      return *found;
    }
    return std::nullopt;
  }

public:
  template <typename T>
  void add(const std::function<void(T&)>& callback) {
    if (tryGetCallback<T>()) {
      throw std::runtime_error("Transformation already registered for this type");
    }
    mCallbacks.push_back(callback);
  }

  template <typename T, typename TCallback>
  void add(const TCallback& callback) {
    add(std::function<void(T&)>(callback));
  }

  template <typename T>
  T& operator ()(T& value) const {
    auto callback = tryGetCallback<T>();
    if (callback) {
      (*callback)(value);
    }
    return value;
  }
};

}
