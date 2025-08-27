#pragma once

#include <cassert>
#include <istream>
#include <string_view>
#include <boost/core/noncopyable.hpp>

namespace pep {

template <typename THash>
class Hasher : private boost::noncopyable {
public:
  using Hash = THash;

private:
  bool mFinished = false;

protected:
  virtual void process(const void* block, size_t size) = 0;
  virtual Hash finish() = 0;

public:
  virtual ~Hasher() noexcept = default;

  Hash digest() {
    assert(!mFinished);
    mFinished = true;
    return this->finish();
  }

  Hasher& update(const void* block, size_t size) {
    assert(!mFinished);
    this->process(block, size);
    return *this;
  }

  Hasher& update(std::string_view data) {
    return this->update(data.data(), data.size());
  }

  Hasher& update(std::istream& source) {
    constexpr std::streamsize HASH_CHUNK_LENGTH{4096};

    char chunk[HASH_CHUNK_LENGTH];
    std::streamsize actual;
    do {
      source.read(chunk, HASH_CHUNK_LENGTH);
      if (source) {
        actual = HASH_CHUNK_LENGTH;
      }
      else if (source.rdstate() == (std::ios_base::eofbit | std::ios_base::failbit)) {
        actual = source.gcount();
      }
      else {
        throw std::runtime_error("Read failure on data stream");
      }
      if (actual > 0) {
        auto converted = static_cast<std::make_unsigned_t<std::streamsize>>(actual);
        this->update(chunk, size_t{converted});
      }
    } while (actual == HASH_CHUNK_LENGTH);

    return *this;
  }

  // Args should be one or more const string&s
  template <typename... Args>
  Hasher& update(std::string_view initial, Args&&... followup) {
    this->update(initial);
    return this->update(std::forward<Args>(followup)...);
  }

  template <typename... Args>
  Hash digest(Args&&... lastPieces) {
    return this->update(std::forward<Args>(lastPieces)...)
      .digest();
  }
};

}
