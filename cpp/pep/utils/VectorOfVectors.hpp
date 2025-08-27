#pragma once

#include <optional>
#include <stdexcept>
#include <vector>

namespace pep {

template <typename T>
class VectorOfVectors {
private:
  std::vector<std::vector<T>> mItems;
  size_t mSize{};

public:
  class const_iterator;

  const_iterator begin() const { return const_iterator(mItems.begin(), mItems.end()); }
  const_iterator end() const { return const_iterator(mItems.end(), mItems.end()); }

  void clear() noexcept {
    mItems.clear();
    mSize = 0U;
  }

  VectorOfVectors& operator +=(std::vector<T> v) {
    if (!v.empty()) {
      mSize += mItems.emplace_back(std::move(v)).size();
    }
    return *this;
  }

  size_t size() const noexcept {
    return mSize;
  }
};

template <typename T>
class VectorOfVectors<T>::const_iterator {
  friend class VectorOfVectors<T>;

private:
  using VectorIterator = typename decltype(VectorOfVectors<T>::mItems)::const_iterator;
  VectorIterator mPosition;
  VectorIterator mEnd;

  using ItemIterator = typename std::vector<T>::const_iterator;
  std::optional<ItemIterator> mItem;

  void placeOnVectorBegin() {
    if (mPosition == mEnd) {
      mItem = std::nullopt;
    }
    else {
      mItem = mPosition->begin();
    }
  }

  const_iterator(VectorIterator position, VectorIterator end)
    : mPosition(std::move(position)), mEnd(std::move(end)) {
    this->placeOnVectorBegin();
  }

public:
  const T& operator*() const {
    assert(mItem.has_value());
    return **mItem;
  }

  const T* operator->() const { return &**this; }

  const_iterator& operator++() {
    // Move to next item within current vector
    assert(mItem.has_value());
    auto& i = *mItem;
    ++i;

    // If we moved past current vector's end, move to next vector
    if (i == mPosition->end()) {
      ++mPosition;
      this->placeOnVectorBegin();
    }

    return *this;
  }

  bool operator==(const const_iterator& other) const noexcept {
    return mPosition == other.mPosition && mItem == other.mItem;
  }

  bool operator!=(const const_iterator& other) const noexcept { return !(*this == other); }
};

}
