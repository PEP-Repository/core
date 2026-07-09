#include <pep/utils/SafePath.hpp>

#include <pep/utils/File.hpp>
#include <pep/utils/Log.hpp>

namespace pep {

void SafePath::ensureNonEmpty() const {
  if (uncheckedPath().empty()) {
    throw std::invalid_argument("Encountered empty path");
  }
}

SafePath SafePath::parentDirectory() const {
  return SafePath::FromTrusted(GetParentDirectory(uncheckedPath()));
}

SafePath SafePath::operator+(const std::filesystem::path& suffix) const {
  auto newPath = uncheckedPath();
  newPath.replace_filename(fileName() + suffix);
  return SafePath::FromTrusted(newPath);
}

SafeRelativePath::SafeRelativePath(std::filesystem::path path)
    : SafePath(ConstructFromTrusted, std::move(path)) {
  if (!IsLexicallyRelativeChildPath(uncheckedPath(), true)) {
    throw std::invalid_argument("Invalid relative path: " + Logging::Escape(text()));
  }
}

SafeRelativeFilePath::SafeRelativeFilePath(std::filesystem::path path)
    : SafeRelativePath(ConstructFromTrusted, std::move(path)) {
  if (!IsLexicallyRelativeChildPath(uncheckedPath(), false)) {
    throw std::invalid_argument("Invalid relative file path: " + Logging::Escape(text()));
  }
}

SafeFileName::SafeFileName(std::filesystem::path fileName)
    : SafeRelativeFilePath(ConstructFromTrusted, std::move(fileName)) {
  auto str = uncheckedPath().string();
  if (!IsValidPlatformFileName(str)) {
    throw std::invalid_argument("Invalid file name: " + Logging::Escape(str));
  }
}

SafeFileName SafeFileName::operator+(const std::filesystem::path& suffix) const {
  auto concat = uncheckedPath();
  concat += suffix;
  return SafeFileName(concat);
}

}
