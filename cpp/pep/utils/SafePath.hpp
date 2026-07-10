#pragma once

#include <concepts>
#include <filesystem>

namespace pep {

class SafeFileName;

/// SafePath is a wrapper to help you guarantee that the contained path is trusted (or empty).
class SafePath {
  std::filesystem::path inner_;

  void finalPathCheck() const;

protected:
  static constexpr struct ConstructFromTrustedTag {} ConstructFromTrusted{};
  explicit SafePath(ConstructFromTrustedTag, std::filesystem::path inner)
    : inner_(std::move(inner)) {}

public:
  SafePath() noexcept = default;

  [[nodiscard]] static SafePath FromTrusted(std::filesystem::path inner) { return SafePath(ConstructFromTrusted, std::move(inner)); }

  /// \see SafeFileName::SafeFileName
  inline SafeFileName fileName() const;

  /// \see GetParentDirectory
  SafePath parentDirectory() const;

  /// This does not check emptiness of the path, prefer using path() where possible.
  const std::filesystem::path& uncheckedPath() const & { return inner_; }

  std::filesystem::path uncheckedPath() && { return std::move(inner_); }

  /// \throws std::invalid_argument if empty.
  const std::filesystem::path& path() const & { finalPathCheck(); return uncheckedPath(); }

  /// \throws std::invalid_argument if empty.
  std::filesystem::path path() && { finalPathCheck(); return std::move(*this).uncheckedPath(); }

  /// \throws std::invalid_argument if empty.
  operator const std::filesystem::path&() const & { return path(); }

  /// \throws std::invalid_argument if empty.
  operator std::filesystem::path() && { return std::move(*this).path(); }

  /// Get string representation, even for empty paths.
  [[nodiscard]] std::string text() const { return uncheckedPath().string(); }

  // Note that for derived classes to be safe, SafePath cannot have modifying operations besides move/copy

  /// \note Either side may be empty, resulting in a trailing slash or no-op.
  [[nodiscard]] SafePath operator/(const SafePath& rhs) const { return FromTrusted(uncheckedPath() / rhs.uncheckedPath()); }

  /// \throws std::invalid_argument if \p suffix contains multiple segments or the result does not have a valid filename for this platform.
  [[nodiscard]] SafePath operator+(const std::filesystem::path& suffix) const;

  [[nodiscard]] auto operator<=>(const SafePath& rhs) const noexcept = default;

  friend std::ostream& operator<<(std::ostream& os, const SafePath& p) {
    return os << p.uncheckedPath();
  }
};

class SafeRelativeFilePath;

/// Relative child path.
///
/// It cannot traverse to the parent directory or contain invalid filenames for this platform.
class SafeRelativePath : public SafePath {
  using SafePath::SafePath;

public:
  SafeRelativePath() noexcept = default;

  /// \throws std::invalid_argument if it traverses to the parent directory or contains invalid filenames for this platform.
  explicit SafeRelativePath(std::filesystem::path path);

  /// \note Either side may be empty, resulting in a trailing slash or no-op.
  [[nodiscard]] SafeRelativePath operator/(const SafeRelativePath& rhs) const {
    return SafeRelativePath(ConstructFromTrusted, uncheckedPath() / rhs.uncheckedPath());
  }
  /// \throws std::invalid_argument if \p rhs is empty.
  [[nodiscard]] inline SafeRelativeFilePath operator/(const SafeRelativeFilePath& rhs) const;
};

/// Relative non-directory child path.
///
/// It cannot have a trailing slash, traverse to the parent directory, or contain invalid filenames for this platform.
class SafeRelativeFilePath : public SafeRelativePath {
  using SafeRelativePath::SafeRelativePath;

public:
  SafeRelativeFilePath() noexcept = default;

  /// \throws std::invalid_argument if it has a trailing slash, traverses to the parent directory, or contains invalid filenames for this platform.
  explicit SafeRelativeFilePath(std::filesystem::path path);
};

/// File name.
///
/// It cannot contain multiple segments and must be a valid filename for this platform.
class SafeFileName : public SafeRelativeFilePath {
  using SafeRelativeFilePath::SafeRelativeFilePath;

public:
  SafeFileName() noexcept = default;

  /// \throws std::invalid_argument if it contains multiple segments or is not a valid filename for this platform.
  explicit SafeFileName(std::filesystem::path fileName);

  /// \throws std::invalid_argument if \p suffix contains multiple segments or the result is not a valid filename for this platform.
  [[nodiscard]] SafeFileName operator+(const std::filesystem::path& suffix) const; //NOLINT(bugprone-derived-method-shadowing-base-method)
};

SafeFileName SafePath::fileName() const { return SafeFileName(uncheckedPath().filename()); }

SafeRelativeFilePath SafeRelativePath::operator/(const SafeRelativeFilePath& rhs) const {
  return SafeRelativeFilePath(ConstructFromTrusted, uncheckedPath() / rhs.path());
}

}

template <std::derived_from<pep::SafePath> Path>
struct std::hash<Path> { // NOLINT(bugprone-std-namespace-modification,cert-dcl58-cpp) Specializing std::hash with our (constrained) type is fine
  [[nodiscard]] size_t operator()(const Path& p) const {
    return std::hash<std::filesystem::path>{}(p);
  }
};
