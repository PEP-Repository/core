#pragma once

#include <concepts>
#include <filesystem>

namespace pep {

class CheckedFileName;

/// CheckedPath is a wrapper to help you guarantee that the contained path is trusted (or empty).
class CheckedPath {
  std::filesystem::path inner_;

  void ensureNonEmpty() const;

protected:
  static constexpr struct ConstructFromTrustedTag {} ConstructFromTrusted{};
  explicit CheckedPath(ConstructFromTrustedTag, std::filesystem::path inner)
    : inner_(std::move(inner)) {}

  /// This does not check emptiness of the path, prefer using path() where possible.
  [[nodiscard]] const std::filesystem::path& uncheckedPath() const & { return inner_; }
  [[nodiscard]] std::filesystem::path uncheckedPath() && { return std::move(inner_); }

public:
  CheckedPath() noexcept = default;

  [[nodiscard]] static CheckedPath FromTrusted(std::filesystem::path inner) { return CheckedPath(ConstructFromTrusted, std::move(inner)); }

  [[nodiscard]] bool empty() const { return uncheckedPath().empty(); }

  /// \see CheckedFileName::CheckedFileName
  inline CheckedFileName fileName() const;

  /// \see GetParentDirectory
  CheckedPath parentDirectory() const;

  /// \throws std::invalid_argument if empty.
  const std::filesystem::path& path() const & { ensureNonEmpty(); return uncheckedPath(); }

  /// \throws std::invalid_argument if empty.
  std::filesystem::path path() && { ensureNonEmpty(); return std::move(*this).uncheckedPath(); }

  /// \throws std::invalid_argument if empty.
  operator const std::filesystem::path&() const & { return path(); }

  /// \throws std::invalid_argument if empty.
  operator std::filesystem::path() && { return std::move(*this).path(); }

  /// Get string representation, even for empty paths.
  ///
  /// Mostly meant for printing paths.
  [[nodiscard]] std::string text() const { return uncheckedPath().string(); }

  // Note that for derived classes to be safe, CheckedPath cannot have modifying operations besides move/copy

  /// \note Either side may be empty, resulting in a trailing slash or no-op.
  [[nodiscard]] CheckedPath operator/(const CheckedPath& rhs) const { return FromTrusted(uncheckedPath() / rhs.uncheckedPath()); }

  /// \throws std::invalid_argument if \p suffix contains multiple elements or the result does not have a valid filename for this platform.
  [[nodiscard]] CheckedPath operator+(const std::filesystem::path& suffix) const;

  [[nodiscard]] auto operator<=>(const CheckedPath& rhs) const noexcept = default;

  friend std::ostream& operator<<(std::ostream& os, const CheckedPath& p) {
    return os << p.uncheckedPath();
  }
};

class CheckedRelativeFilePath;

/// Relative child path.
///
/// It cannot traverse to the parent directory or contain invalid filenames for this platform.
class CheckedRelativePath : public CheckedPath {
  using CheckedPath::CheckedPath;

public:
  CheckedRelativePath() noexcept = default;

  /// \throws std::invalid_argument if it traverses to the parent directory or contains invalid filenames for this platform.
  explicit CheckedRelativePath(std::filesystem::path path);

  /// \note Either side may be empty, resulting in a trailing slash or no-op.
  [[nodiscard]] CheckedRelativePath operator/(const CheckedRelativePath& rhs) const {
    return CheckedRelativePath(ConstructFromTrusted, uncheckedPath() / rhs.uncheckedPath());
  }
  /// \throws std::invalid_argument if \p rhs is empty.
  [[nodiscard]] inline CheckedRelativeFilePath operator/(const CheckedRelativeFilePath& rhs) const;
};

/// Relative non-directory child path.
///
/// It cannot have a trailing slash, traverse to the parent directory, or contain invalid filenames for this platform.
class CheckedRelativeFilePath : public CheckedRelativePath {
  using CheckedRelativePath::CheckedRelativePath;

public:
  CheckedRelativeFilePath() noexcept = default;

  /// \throws std::invalid_argument if it has a trailing slash, traverses to the parent directory, or contains invalid filenames for this platform.
  explicit CheckedRelativeFilePath(std::filesystem::path path);
};

/// File name.
///
/// It cannot contain multiple elements and must be a valid filename for this platform.
class CheckedFileName : public CheckedRelativeFilePath {
  using CheckedRelativeFilePath::CheckedRelativeFilePath;

public:
  CheckedFileName() noexcept = default;

  /// \throws std::invalid_argument if it contains multiple elements or is not a valid filename for this platform.
  explicit CheckedFileName(std::filesystem::path fileName);

  /// \throws std::invalid_argument if \p suffix contains multiple elements or the result is not a valid filename for this platform.
  [[nodiscard]] CheckedFileName operator+(const std::filesystem::path& suffix) const; //NOLINT(bugprone-derived-method-shadowing-base-method)
};

CheckedFileName CheckedPath::fileName() const { return CheckedFileName(uncheckedPath().filename()); }

CheckedRelativeFilePath CheckedRelativePath::operator/(const CheckedRelativeFilePath& rhs) const {
  return CheckedRelativeFilePath(ConstructFromTrusted, uncheckedPath() / rhs.path());
}

}

template <std::derived_from<pep::CheckedPath> Path>
struct std::hash<Path> { // NOLINT(bugprone-std-namespace-modification,cert-dcl58-cpp) Specializing std::hash with our (constrained) type is fine
  [[nodiscard]] size_t operator()(const Path& p) const {
    return std::hash<std::filesystem::path>{}(p);
  }
};
