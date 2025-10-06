#include <pep/utils/Filesystem.hpp>
#include <cassert>
#include <random>

namespace pep::filesystem {

Temporary::~Temporary() noexcept {
  remove_all(mPath); // noop if mPath does not exist or if it's empty
}

Temporary& Temporary::operator=(Temporary&& rhs) noexcept {
  if (rhs.mPath != mPath) {
    remove_all(mPath);
  }
  mPath = rhs.release();
  return *this;
}

std::filesystem::path Temporary::release() {
  std::filesystem::path p{};
  std::swap(p, mPath);
  return p;
}

std::string RandomizedName(std::string str) {
  constexpr auto SPECIAL_CHAR = '%';
  constexpr std::string_view AVAILABLE_CHARS = "abcdefghijklmnopqrstuvwxyz0123456789";

  auto randomChar = [&AVAILABLE_CHARS,
                     engine = std::default_random_engine(std::random_device{}()),
                     distribution =
                         std::uniform_int_distribution<std::size_t>(0, AVAILABLE_CHARS.size() - 1)]() mutable {
    return AVAILABLE_CHARS[distribution(engine)];
  };

  for (auto& c : str) {
    c = (c != SPECIAL_CHAR) ? c : randomChar();
  }
  return str;
}

std::pair<SetOfExistingPaths::const_iterator, bool> SetOfExistingPaths::insert(const std::filesystem::path& path) {
  // Store _canonical_ paths (a.o.) to ensure that differences in cAsInG don't affect the comparison (if the file system is case insensitive, e.g. on Windows)
  auto raw = mImplementor.insert(canonical(path));
  return { raw.first, raw.second };
}

bool SetOfExistingPaths::contains(const std::filesystem::path& path) const {
  // Use "weakly_canonical" to prevent exceptions if path doesn't exist on file system
  return mImplementor.contains(weakly_canonical(path));
}

} // namespace pep::filesystem
