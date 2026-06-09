#include <pep/utils/Hasher.hpp>

#include <array>
#include <cassert>

namespace pep {

void HasherBase::update(const void* block, size_t size) {
  assert(!mFinished);
  this->process(block, size);
}

void HasherBase::update(std::istream& source) {
  constexpr std::streamsize HASH_CHUNK_LENGTH{4096};

  std::array<char, HASH_CHUNK_LENGTH> chunk{};
  std::streamsize actual{-1};
  do {
    source.read(chunk.data(), chunk.size());
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
      this->update(chunk.data(), size_t{converted});
    }
  } while (actual == HASH_CHUNK_LENGTH);
}

void HasherBase::setFinished() {
  assert(!mFinished);
  mFinished = true;
}

}
