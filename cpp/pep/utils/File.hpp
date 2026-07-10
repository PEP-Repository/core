#pragma once

#include <functional>
#include <optional>
#include <filesystem>


namespace pep {

std::string ReadFile(const std::filesystem::path& path);
std::optional<std::string> ReadFileIfExists(const std::filesystem::path& path);
void WriteFile(const std::filesystem::path& path, const std::string& content);
[[nodiscard]] bool IsValidFileExtension(const std::string& extension);

[[nodiscard]] bool IsValidUnixFileName(std::string_view name);
[[nodiscard]] bool IsValidWindowsFileName(std::string_view name);
/// Is valid file name for the current platform?
[[nodiscard]] bool IsValidPlatformFileName(std::string_view name);
/// Is valid file name for all supported platforms?
[[nodiscard]] bool IsValidPortableFileName(std::string_view name);

/* \brief Reads a std::istream, iteratively filling a buffer. The writeToDestination lambda function can take this buffer (as char* and its length) and
 * define what should be done with it.
 */
void IstreamToDestination(std::istream& in, std::function<void(const char* c, const std::streamsize len)> writeToDestination);

/// Returns if \p path is a relative path not outside the reference folder.
/// E.g. when normalized it does not contain "..".
/// Also checks IsValidPlatformFileName for each non-empty non-dot segment.
/// Does not resolve symlinks.
/// \param allowDirectories Disallow "." or trailing "/"
[[nodiscard]] bool IsLexicallyRelativeChildPath(const std::filesystem::path& path, bool allowDirectories = true);

/// Strips trailing directory separator, if possible.
[[nodiscard]] std::filesystem::path StripTrailingSlash(const std::filesystem::path& path);

/// \brief Returns parent directory path.
/// \details Returns parent directory path. This is different from \c std::filesystem::path::parent_path,
///   which just strips the last segment, even if it is empty or a dot.
///   Does not make path absolute. Result may contain "." or "..".
/// \throws std::invalid_argument if the relative path element is empty.
std::filesystem::path GetParentDirectory(const std::filesystem::path& path);

/// Make path absolute, then append a suffix (e.g. "-pending") to the filename of a directory path.
[[nodiscard]] std::filesystem::path AppendDirectoryNameSuffix(const std::filesystem::path& path, std::string_view suffix);

}


