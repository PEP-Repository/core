#pragma once

#include <concepts>
#include <filesystem>

namespace pep {

class SafeFileName;

// Note that for derived classes to be safe, SafePath cannot have modifying operations besides move/copy
class SafePath {
protected:
  std::filesystem::path inner_;

  void finalPathCheck() const;

  static constexpr struct fromTrustedTag {} fromTrusted{};
  explicit SafePath(fromTrustedTag, std::filesystem::path inner)
    : inner_(std::move(inner)) {}

public:
  SafePath() noexcept = default;

  [[nodiscard]] static SafePath FromTrusted(std::filesystem::path inner) { return SafePath(fromTrusted, std::move(inner)); }

  /// \see SafeFileName::SafeFileName
  inline SafeFileName fileName() const;

  /// \see GetParentDirectory
  SafePath parentDirectory() const;

  /// \throws std::invalid_argument if empty.
  const std::filesystem::path& path() const & { finalPathCheck(); return inner_; }

  /// \throws std::invalid_argument if empty.
  std::filesystem::path path() && { finalPathCheck(); return std::move(inner_); }

  /// \throws std::invalid_argument if empty.
  operator const std::filesystem::path&() const & { return path(); }

  /// \throws std::invalid_argument if empty.
  operator std::filesystem::path() && { return std::move(*this).path(); }

  /// Get string representation, even for empty paths.
  [[nodiscard]] std::string text() { return inner_.string(); }

  [[nodiscard]] SafePath operator/(const SafePath& rhs) const { return FromTrusted(inner_ / rhs.inner_); }

  /// \throws std::invalid_argument if \p suffix contains multiple segments or the result is not a valid filename for this platform.
  [[nodiscard]] SafePath operator+(const std::filesystem::path& suffix) const;

  [[nodiscard]] auto operator<=>(const SafePath& rhs) const noexcept = default;

  friend std::ostream& operator<<(std::ostream& os, const SafePath& p) {
    return os << p.inner_;
  }
};

class SafeRelativeFilePath;

/// Relative child path.
class SafeRelativePath : public SafePath {
  using SafePath::SafePath;

public:
  SafeRelativePath() noexcept = default;

  /// \throws std::invalid_argument if it traverses to the parent directory or contains invalid filenames for this platform.
  explicit SafeRelativePath(std::filesystem::path path);

  [[nodiscard]] SafeRelativePath operator/(const SafeRelativePath& rhs) const { return SafeRelativePath(fromTrusted, inner_ / rhs.inner_); }
  /// \throws std::invalid_argument if \p rhs is empty.
  [[nodiscard]] inline SafeRelativeFilePath operator/(const SafeRelativeFilePath& rhs);
};

/// Relative non-directory child path.
class SafeRelativeFilePath : public SafeRelativePath {
  using SafeRelativePath::SafeRelativePath;

public:
  SafeRelativeFilePath() noexcept = default;

  /// \throws std::invalid_argument if it has a trailing slash, traverses to the parent directory, or contains invalid filenames for this platform.
  explicit SafeRelativeFilePath(std::filesystem::path path);
};

class SafeFileName : public SafeRelativeFilePath {
  using SafeRelativeFilePath::SafeRelativeFilePath;

public:
  SafeFileName() noexcept = default;

  /// \throws std::invalid_argument if it contains multiple segments or is not a valid filename for this platform.
  explicit SafeFileName(std::filesystem::path fileName);

  /// \throws std::invalid_argument if \p suffix contains multiple segments or the result is not a valid filename for this platform.
  [[nodiscard]] SafeFileName operator+(const std::filesystem::path& suffix) const; //NOLINT(bugprone-derived-method-shadowing-base-method)
};

SafeFileName SafePath::fileName() const { return SafeFileName(inner_.filename()); }
  
SafeRelativeFilePath SafeRelativePath::operator/(const SafeRelativeFilePath& rhs) {
  return SafeRelativeFilePath(fromTrusted, inner_ / rhs.path());
}

}

template <std::derived_from<pep::SafePath> Path>
struct std::hash<Path> {
  [[nodiscard]] size_t operator()(const Path& p) const {
    return std::hash<std::filesystem::path>{}(p);
  }
};
