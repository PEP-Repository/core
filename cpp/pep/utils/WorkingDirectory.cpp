#include <pep/utils/WorkingDirectory.hpp>
#include <pep/utils/Win32Api.hpp>
#include <cassert>

#ifdef __linux__
#include <linux/limits.h>
#endif

namespace pep {

std::filesystem::path GetWorkingDirectory() {
  std::filesystem::path result;

#ifdef _WIN32
  char buffer[FILENAME_MAX];
  if (::GetCurrentDirectoryA(FILENAME_MAX, buffer) == 0) {
    win32api::ApiCallFailure::RaiseLastError();
  }
  result = buffer;
#else
  char buffer[PATH_MAX];
  if (getcwd(buffer, PATH_MAX) == nullptr) {
    throw std::runtime_error("Could not get working directory");
  }
  result = buffer;
#endif

  assert(std::filesystem::is_directory(result));
  return result;
}

void SetWorkingDirectory(const std::filesystem::path& directory) {
  auto dir = directory.string();
#ifdef _WIN32
  if (::SetCurrentDirectoryA(dir.data()) == 0) {
    win32api::ApiCallFailure::RaiseLastError();
  }
#else
  if (chdir(dir.data()) != 0) {
    throw std::runtime_error("Could not set working directory");
  }
#endif
}

}
