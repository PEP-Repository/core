#include <pep/utils/CheckedPath.hpp>

#include <pep/utils/File.hpp>
#include <pep/utils/Log.hpp>

namespace pep {

void CheckedPath::ensureNonEmpty() const {
  if (uncheckedPath().empty()) {
    throw std::invalid_argument("Encountered empty path");
  }
}

CheckedPath CheckedPath::parentDirectory() const {
  return CheckedPath::FromTrusted(GetParentDirectory(uncheckedPath()));
}

CheckedPath CheckedPath::operator+(const std::filesystem::path& suffix) const {
  auto newPath = uncheckedPath();
  newPath.replace_filename(fileName() + suffix);
  return CheckedPath::FromTrusted(newPath);
}

CheckedRelativePath::CheckedRelativePath(std::filesystem::path path)
    : CheckedPath(ConstructFromTrusted, std::move(path)) {
  if (!IsLexicallyRelativeChildPath(uncheckedPath(), true)) {
    throw std::invalid_argument("Invalid relative path: " + Logging::Escape(text()));
  }
}

CheckedRelativeFilePath::CheckedRelativeFilePath(std::filesystem::path path)
    : CheckedRelativePath(ConstructFromTrusted, std::move(path)) {
  if (!IsLexicallyRelativeChildPath(uncheckedPath(), false)) {
    throw std::invalid_argument("Invalid relative file path: " + Logging::Escape(text()));
  }
}

CheckedFileName::CheckedFileName(std::filesystem::path fileName)
    : CheckedRelativeFilePath(ConstructFromTrusted, std::move(fileName)) {
  auto str = uncheckedPath().string();
  if (!IsValidPlatformFileName(str)) {
    throw std::invalid_argument("Invalid file name: " + Logging::Escape(str));
  }
}

CheckedFileName CheckedFileName::operator+(const std::filesystem::path& suffix) const {
  auto concat = uncheckedPath();
  concat += suffix;
  return CheckedFileName(concat);
}

}
