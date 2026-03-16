#pragma once

#include <istream>
#include <string_view>
#include <boost/core/noncopyable.hpp>

namespace pep {

namespace detail {
class HasherBase : private boost::noncopyable {
  bool mFinished = false;

protected:
  void update(const void* block, size_t size);
  void update(std::istream& source);
  void setFinished();

  virtual void process(const void* block, size_t size) = 0;

public:
  virtual ~HasherBase() noexcept = default;
};
}

template <typename THash>
class Hasher : public detail::HasherBase {
public:
  using Hash = THash;

protected:
  virtual Hash finish() = 0;

public:
  [[nodiscard]] Hash digest() {
    this->setFinished();
    return this->finish();
  }

  Hasher& update(const void* block, size_t size) {
    HasherBase::update(block, size);
    return *this;
  }

  Hasher& update(std::string_view data) {
    return this->update(data.data(), data.size());
  }

  Hasher& update(std::istream& source) {
    HasherBase::update(source);
    return *this;
  }

  // Args should be one or more const string&s
  template <typename... Args>
  Hasher& update(std::string_view initial, Args&&... followup) {
    this->update(initial);
    return this->update(std::forward<Args>(followup)...);
  }

  template <typename... Args>
  [[nodiscard]] Hash digest(Args&&... lastPieces) {
    return this->update(std::forward<Args>(lastPieces)...)
      .digest();
  }
};

}
