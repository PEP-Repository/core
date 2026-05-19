#include <pep/utils/SafePath.hpp>

#include <pep/utils/File.hpp>
#include <pep/utils/Log.hpp>

namespace pep {

void SafePath::finalPathCheck() const {
  if (inner_.empty()) {
    throw std::invalid_argument("Encountered empty path");
  }
}

SafePath SafePath::parentDirectory() const {
  return SafePath::FromTrusted(GetParentDirectory(inner_));
}

SafePath SafePath::operator+(const std::filesystem::path& suffix) const {
  auto newPath = inner_;
  newPath.replace_filename(fileName() + suffix);
  return SafePath::FromTrusted(newPath);
}

SafeRelativePath::SafeRelativePath(std::filesystem::path path)
    : SafePath(fromTrusted, std::move(path)) {
  if (!IsLexicallyRelativeChildPath(inner_, true)) {
    throw std::invalid_argument("Invalid relative path: " + Logging::Escape(inner_.string()));
  }
}

SafeRelativeFilePath::SafeRelativeFilePath(std::filesystem::path path)
    : SafeRelativePath(fromTrusted, std::move(path)) {
  if (!IsLexicallyRelativeChildPath(inner_, false)) {
    throw std::invalid_argument("Invalid relative file path: " + Logging::Escape(inner_.string()));
  }
}

SafeFileName::SafeFileName(std::filesystem::path fileName)
    : SafeRelativeFilePath(fromTrusted, std::move(fileName)) {
  auto str = inner_.string();
  if (!IsValidPlatformFileName(str)) {
    throw std::invalid_argument("Invalid file name: " + Logging::Escape(str));
  }
}

SafeFileName SafeFileName::operator+(const std::filesystem::path& suffix) const {
  auto concat = inner_;
  concat += suffix;
  return SafeFileName(concat);
}

}
