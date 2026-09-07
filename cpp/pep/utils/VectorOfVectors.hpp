#pragma once

#include <optional>
#include <stdexcept>
#include <vector>

namespace pep {

template <typename T>
class VectorOfVectors {
private:
  std::vector<std::vector<T>> items_;
  size_t size_{};

public:
  class const_iterator;

  const_iterator begin() const { return const_iterator(items_.begin(), items_.end()); }
  const_iterator end() const { return const_iterator(items_.end(), items_.end()); }

  void clear() noexcept {
    items_.clear();
    size_ = 0U;
  }

  VectorOfVectors& operator +=(std::vector<T> v) {
    if (!v.empty()) {
      size_ += items_.emplace_back(std::move(v)).size();
    }
    return *this;
  }

  size_t size() const noexcept {
    return size_;
  }
};

template <typename T>
class VectorOfVectors<T>::const_iterator {
  friend class VectorOfVectors<T>;

private:
  using VectorIterator = typename decltype(VectorOfVectors<T>::items_)::const_iterator;
  VectorIterator position_;
  VectorIterator end_;

  using ItemIterator = typename std::vector<T>::const_iterator;
  std::optional<ItemIterator> item_;

  void placeOnVectorBegin() {
    if (position_ == end_) {
      item_ = std::nullopt;
    }
    else {
      item_ = position_->begin();
    }
  }

  const_iterator(VectorIterator position, VectorIterator end)
    : position_(std::move(position)), end_(std::move(end)) {
    this->placeOnVectorBegin();
  }

public:
  const T& operator*() const {
    assert(item_.has_value());
    return **item_;
  }

  const T* operator->() const { return &**this; }

  const_iterator& operator++() {
    // Move to next item within current vector
    assert(item_.has_value());
    auto& i = *item_;
    ++i;

    // If we moved past current vector's end, move to next vector
    if (i == position_->end()) {
      ++position_;
      this->placeOnVectorBegin();
    }

    return *this;
  }

  bool operator==(const const_iterator& other) const noexcept {
    return position_ == other.position_ && item_ == other.item_;
  }

  bool operator!=(const const_iterator& other) const noexcept { return !(*this == other); }
};

}
