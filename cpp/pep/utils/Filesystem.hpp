#pragma once

#include <filesystem>
#include <set>

namespace pep::filesystem {
/// Combines std::filesystem with our own filesystem extensions.

/// Binds the lifetime of a filesystem resource to an object,
/// automatically deleting the resource when the object is deleted.
class Temporary final {
public:
  /// Creates an empty object, that is not bound to any filesystem object.
  Temporary() = default;

  /// Binds the lifetime of the resource located at \p path to the constructed object.
  explicit Temporary(std::filesystem::path path) : mPath(std::move(path)) {}

  /// Deletes the bounded resource from disk if it exists.
  /// @warning Results in termination of the application if the underlying system calls fail.
  ~Temporary() noexcept;
  /// Reassignment deletes the currently bound resource.
  Temporary& operator=(Temporary&&) noexcept;

  Temporary(Temporary&&) = default; // moveable

  Temporary(const Temporary&) = delete; // not copyable
  Temporary& operator=(const Temporary&) = delete;

  /// The path to the managed resource.
  const std::filesystem::path& path() const {
    return mPath;
  }

  /// Decouples the filesystem resource from this object and returns its path.
  std::filesystem::path release();

  /// Returns true iff the current path is empty.
  bool empty() const noexcept {
    return mPath.empty();
  }

  bool operator==(const Temporary&) const = default;
  bool operator!=(const Temporary&) const = default;

private:
  std::filesystem::path mPath;
};

/// Returns a string where every occurrence of '%' in the \p pattern is replaced with a randomized character.
/// The randomized characters are lowercase letters or digits.
///
/// Replacement characters are randomly selected lower case alpha characters or digits.
/// This makes the chance that two call produce the same result 36^(-n),
/// with n being the count of '%' chars in the pattern.
///
/// @remark Quick reference for the chance that two consecutive calls return the same value for specific '%' counts:
///         <ul>
///           <li>count('%') == 3 : approx 1 in        46'000</li>
///           <li>count('%') == 4 : approx 1 in     1'700'000</li>
///           <li>count('%') == 5 : approx 1 in    60'000'000</li>
///           <li>count('%') == 6 : approx 1 in 2'200'000'000</li>
///         </ul>
///
/// @warning Do not trust this to generate unique names when a naming collision would have catastrophic results,
///          such as causing the crash of an application in a production environment or
///          resulting in the loss of (potentially unrecoverable) data.
std::string RandomizedName(std::string pattern);
inline std::string RandomizedName(std::string_view pattern) {
  return RandomizedName(std::string{pattern});
}
inline std::string RandomizedName(const char* pattern) {
  return RandomizedName(std::string{pattern});
}

/// @brief Set of std::filesystem::path instances that exist on the file system (at the time they are added)
class SetOfExistingPaths {
private:
  std::set<std::filesystem::path> mImplementor;

public:
  /// @brief A const iterator into the set
  using const_iterator = decltype(mImplementor)::const_iterator;

  /// @brief Ensures that the set contains the specified path
  /// @param path The path to include
  /// @return A pair of (1) an iterator pointing to the path's position in the set and (2) a bool indicating whether the path was added as a result of the call
  /// @remark Caller must ensure that the path actually does exist
  std::pair<const_iterator, bool> insert(const std::filesystem::path& path);

  /// @brief Produces a const_iterator positioned on the set's first item
  /// @return A const_iterator positioned on the set's first item
  const_iterator begin() const { return mImplementor.begin(); }

  /// @brief Produces a const_iterator positioned beyond the set's last item
  /// @return A const_iterator positioned beyond the set's last item
  const_iterator end() const { return mImplementor.end(); }

  /// @brief Returns the number of items in the set
  /// @return The number of items in the set
  size_t size() const { return mImplementor.size(); }

  /// @brief Determines if the set contains the specified path
  /// @param path The path to check
  /// @return TRUE if the set contains the path; FALSE if not or if the specified path doesn't exist
  bool contains(const std::filesystem::path& path) const;
};

// We deliberately put this at the end of the file so that we are forced to
// fully qualify all references to std::filesystem in the definitions of our extensions.
using namespace std::filesystem;

} // namespace pep::filesystem
