#pragma once

#include <concepts>
#include <filesystem>

namespace pep {

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
  inline class SafeFileName fileName() const;

  /// \see GetParentDirectory
  class SafePath parentDirectory() const;

  /// \throws std::runtime_error if empty.
  const std::filesystem::path& path() const & { finalPathCheck(); return inner_; }

  /// \throws std::runtime_error if empty.
  std::filesystem::path path() && { finalPathCheck(); return std::move(inner_); }

  /// \throws std::runtime_error if empty.
  operator const std::filesystem::path&() const & { return path(); }

  /// \throws std::runtime_error if empty.
  operator std::filesystem::path() && { return std::move(*this).path(); }

  /// Get string representation, even for empty paths.
  [[nodiscard]] std::string text() { return inner_.string(); }

  /// \throws std::runtime_error if either side is empty.
  [[nodiscard]] SafePath operator/(const SafePath& rhs) const { return SafePath::FromTrusted(path() / rhs.path()); }

  /// \throws std::runtime_error if \p suffix contains multiple segments or the result is not a valid filename for this platform.
  [[nodiscard]] SafePath operator+(const std::filesystem::path& suffix) const;

  [[nodiscard]] auto operator<=>(const SafePath& rhs) const noexcept = default;

  friend std::ostream& operator<<(std::ostream& os, const SafePath& p) {
    return os << p.inner_;
  }
};

/// Relative child path.
class SafeRelativePath : public SafePath {
  using SafePath::SafePath;

public:
  SafeRelativePath() noexcept = default;

  /// \throws std::runtime_error if it traverses to the parent directory or contains invalid filenames for this platform.
  explicit SafeRelativePath(std::filesystem::path path);

  /// \throws std::runtime_error if either side is empty.
  [[nodiscard]] SafeRelativePath operator/(const SafeRelativePath& rhs) const { return SafeRelativePath(fromTrusted, path() / rhs.path()); }
};

/// Relative non-directory child path.
class SafeRelativeFilePath : public SafeRelativePath {
  using SafeRelativePath::SafeRelativePath;

public:
  SafeRelativeFilePath() noexcept = default;

  /// \throws std::runtime_error if it has a trailing slash, traverses to the parent directory, or contains invalid filenames for this platform.
  explicit SafeRelativeFilePath(std::filesystem::path path);

  [[nodiscard]] friend SafeRelativeFilePath operator/(const SafeRelativePath& lhs, const SafeRelativeFilePath& rhs) { return SafeRelativeFilePath(fromTrusted, lhs.path() / rhs.path()); }
};

class SafeFileName : public SafeRelativeFilePath {
  using SafeRelativeFilePath::SafeRelativeFilePath;

public:
  SafeFileName() noexcept = default;

  /// \throws std::runtime_error if it contains multiple segments or is not a valid filename for this platform.
  explicit SafeFileName(std::filesystem::path fileName);

  /// \throws std::runtime_error if \p suffix contains multiple segments or the result is not a valid filename for this platform.
  [[nodiscard]] SafeFileName operator+(const std::filesystem::path& suffix) const;
};

SafeFileName SafePath::fileName() const { return SafeFileName(inner_.filename()); }

}

template <std::derived_from<pep::SafePath> Path>
struct std::hash<Path> {
  [[nodiscard]] size_t operator()(const Path& p) const {
    return std::hash<std::filesystem::path>{}(p);
  }
};
