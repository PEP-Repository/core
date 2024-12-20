#pragma once

#include <filesystem>

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

// We deliberately put this at the end of the file so that we are forced to
// fully qualify all references to std::filesystem in the definitions of our extensions.
using namespace std::filesystem;

} // namespace pep::filesystem
