#include <pep/utils/File.hpp>

#include <pep/utils/Log.hpp>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <fstream>
#include <ranges>
#include <regex>
#include <cassert>

using namespace std::literals;

namespace pep {

namespace {

const std::streamsize DISK_IO_BUFFERSIZE{ 4096 };

const std::array<std::string, 28> globalWindowsDeviceNames{
  "CON", "PRN", "AUX", "NUL",
  // Also include ISO/IEC 8859-1 superscript numbers '\xb9' etc.
  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "COM\xb9", "COM\xb2", "COM\xb3",
  "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9", "LPT\xb9", "LPT\xb2", "LPT\xb3"
};

}

std::string ReadFile(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("File " + std::filesystem::absolute(path).string() + " does not exist");
  }

  std::ifstream in(path, std::ios::in | std::ios::binary | std::ios::ate); // Binary mode prevents Windows from stopping when it encounters 0x1A (Ctrl+Z). See #1055
  in.exceptions(in.exceptions() | std::ios::failbit);
  auto size = in.tellg();
  assert(size >= 0);
  in.seekg(0);
  std::string content(static_cast<std::string::size_type>(size), '\0');
  in.read(content.data(), static_cast<std::streamsize>(content.size())); // will throw on failures since we added failbit to the exception mask
  in.close();
  return content;
}

std::optional<std::string> ReadFileIfExists(const std::filesystem::path& path) {
  if (std::filesystem::exists(path)) {
    return ReadFile(path.string());
  }
  return std::nullopt;
}

void WriteFile(const std::filesystem::path& path, const std::string& content) {
  std::ofstream output(path, std::ios::out | std::ios::binary);
  output.exceptions(output.exceptions() | std::ios::failbit);
  output << content;
  output.close();
}

bool IsValidFileExtension(const std::string& extension) {
  const std::regex extensionRegex("(\\.[A-Za-z0-9]+)+");
  return std::regex_match(extension, extensionRegex);
}

bool IsValidUnixFileName(std::string_view name) {
  if (name.empty() || name == "." || name == "..") {
    return false;
  }
  if (name.find_first_of("/\0"sv) != std::string::npos) {
    return false;
  }
  return true;
}

bool IsValidWindowsFileName(std::string_view name) {
  if (!IsValidUnixFileName(name)) {
    return false;
  }

  // See https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file

  // Trailing space / dot would be stripped (if not prefixed by "\\?\"),
  // and are not handled well by most programs including the file Explorer
  if (name.ends_with(' ') || name.ends_with('.')) {
    return false;
  }

  if (name.find_first_of(R"("*:<>?\|)") != std::string::npos) {
    return false;
  }
  using namespace std::ranges;
  if (any_of(name, boost::is_from_range('\0', '\x1F'))) {
    return false;
  }

  // Note: variants with extension like NUL.tar.gz are supported in Windows 11 but not before
  auto nameWithoutExtensions = name.substr(0, name.find('.'));
  if (any_of(globalWindowsDeviceNames, [&](const auto& name) { return boost::iequals(name, nameWithoutExtensions); })) {
    return false;
  }

  // Note: CONIN$ and CONOUT$ also have special meaning when specifying only a file name without path,
  // but with path it's fine.
  // See https://learn.microsoft.com/en-us/windows/console/console-handles

  return true;
}

bool IsValidPlatformFileName(std::string_view name) {
#ifdef _WIN32
  return IsValidWindowsFileName(name);
#else
  return IsValidUnixFileName(name);
#endif
}

bool IsValidPortableFileName(std::string_view name) {
  // IsValidWindowsFileName includes a IsValidUnixFileName check
  return IsValidWindowsFileName(name);
}

void IstreamToDestination(std::istream& in, std::function<void(const char * c, const std::streamsize len)> writeToDestination) {
  while (!in.eof()) {
    if (in.fail()) {
      throw std::runtime_error("Reading from stream failed"); // Exception meant to be rethrown with more information.
    }
    std::array<char, DISK_IO_BUFFERSIZE> buffer{};
    in.read(buffer.data(), buffer.size());
    if (in.gcount() > 0) {
      writeToDestination(buffer.data(), in.gcount());
    }
    else {
      break;
    }
  }
}

bool IsLexicallyRelativeChildPath(const std::filesystem::path& path, bool allowDirectories) {
  // Note that, at least on Windows, a path like "C:foo" is considered relative, but may be on a different drive.
  if (path.empty() || !path.is_relative() || path.has_root_path()) {
    return false;
  }
  using namespace std::ranges;
  if (!all_of(path | views::filter([] (const auto& segment) { return !segment.empty() && segment != "." && segment != ".."; }),
        [] (const auto& segment) { return IsValidPlatformFileName(segment.string()); })) {
    return false;
  }
  const auto normalPath = path.lexically_normal();
  if (*normalPath.begin() == "..") {
    return false;
  }
  if (!allowDirectories && (normalPath == "." || !path.has_filename())) {
    return false;
  }
  return true;
}

std::filesystem::path StripTrailingSlash(const std::filesystem::path& path) {
  if (path.has_relative_path() && !path.has_filename()) {
    return path.parent_path();
  }
  return path;
}

std::filesystem::path GetParentDirectory(const std::filesystem::path& path) {
  auto normalPath = StripTrailingSlash(path.lexically_normal());
  if (!normalPath.has_relative_path()) {
    // Path has e.g. "/", or "\" in "C:\".
    // Note that this is not the same as `is_absolute`, as "\" is not absolute on Windows (it is still relative to the drive).
    if (normalPath.has_root_directory()) {
      throw std::invalid_argument("Cannot get parent directory of root directory: " + Logging::Escape(path.string()));
    } else {
#ifdef _WIN32
      // On Windows, "" is equivalent to "." and "C:" is equivalent to "C:."
      normalPath.replace_filename(".");
#else
      throw std::invalid_argument("Cannot get parent directory for empty relative path: " + Logging::Escape(path.string()));
#endif
    }
  }
  assert(normalPath.has_filename() && "Expected filename after stripping trailing slash when relative path is present");
  const auto filename = normalPath.filename();
  if (filename == ".") {
    normalPath.replace_filename("..");
  } else if (filename == "..") {
    normalPath /= "..";
  } else {
    // If not "." or "..", it's safe to strip off the last component
    // (we already know it's also not empty, see above)
    normalPath = normalPath.parent_path();
    // Prevent returning empty path
    if (normalPath.empty()) {
      normalPath.replace_filename(".");
    }
  }
  return normalPath;
}

std::filesystem::path AppendDirectoryNameSuffix(const std::filesystem::path& path, std::string_view suffix) {
  if (path.empty()) {
    throw std::invalid_argument("Directory path is empty");
  }
  // Make absolute and collapse "." and ".."
  auto normalPath = StripTrailingSlash(absolute(path).lexically_normal());
  if (!normalPath.has_filename()) { // E.g. "/"
    throw std::invalid_argument("Directory path is filesystem root: " + Logging::Escape(path.string()));
  }
  normalPath.replace_filename(normalPath.filename() += suffix);
  return normalPath;
}

}
