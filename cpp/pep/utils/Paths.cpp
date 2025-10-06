#include <pep/utils/Paths.hpp>

#include <boost/dll/runtime_symbol_info.hpp>

namespace pep {

namespace {

constexpr bool Emscripten =
#ifdef __EMSCRIPTEN__
    true
#else
    false
#endif
  ;

std::filesystem::path GetAbsoluteWorkingDirOrCurrentPath(std::filesystem::path absWorkingDir = {}) {
  //NOLINTNEXTLINE(concurrency-mt-unsafe) std::getenv is thread safe as long as we do not setenv/unsetenv/putenv
  if (Emscripten || std::getenv("PEP_USE_CURRENT_PATH")) {
    return std::filesystem::current_path();
  }
  if (!absWorkingDir.empty()) {
    return absWorkingDir;
  }
  return canonical(GetExecutablePath()).parent_path();
}

}

std::filesystem::path GetExecutablePath() {
  return boost::dll::program_location();
}

std::filesystem::path GetResourceWorkingDirForOS() {
  std::filesystem::path workingDir;
  //NOLINTNEXTLINE(concurrency-mt-unsafe) std::getenv is thread safe as long as we do not setenv/unsetenv/putenv
  if (const char* envConfigDir = std::getenv("PEP_CONFIG_DIR")){
    workingDir = envConfigDir;
  }
  else {
#if defined(__APPLE__) && defined(__MACH__)
    workingDir = std::filesystem::canonical(GetExecutablePath()).parent_path().parent_path() / "Resources";
#elif !defined(__EMSCRIPTEN__)
    workingDir = std::filesystem::canonical(GetExecutablePath()).parent_path();
#endif
  }
  return GetAbsoluteWorkingDirOrCurrentPath(workingDir);
}

std::filesystem::path GetAbsolutePath(const std::filesystem::path& p) {
  return GetAbsoluteWorkingDirOrCurrentPath() / p;
}

std::filesystem::path GetLogBasePath() {
  if constexpr (Emscripten) {
    return std::filesystem::current_path() / "log";
  } else {
    return std::filesystem::current_path() / GetExecutablePath().filename();
  }
}

}
