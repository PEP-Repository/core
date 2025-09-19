#include <pep/utils/Paths.hpp>

#include <boost/dll/runtime_symbol_info.hpp>

namespace pep {

namespace {

std::filesystem::path GetAbsoluteWorkingDirOrCurrentPath(std::filesystem::path workingDir) {
  //NOLINTNEXTLINE(concurrency-mt-unsafe) std::getenv is thread safe as long as we do not setenv/unsetenv/putenv
  if (std::getenv("PEP_USE_CURRENT_PATH") == nullptr) {
    if (workingDir.empty()) {
      workingDir = std::filesystem::canonical(GetExecutablePath()).parent_path();
    }
  }
  else {
    workingDir = std::filesystem::current_path();
  }

  return std::filesystem::absolute(workingDir);
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
#else
    workingDir = std::filesystem::canonical(GetExecutablePath()).parent_path();
#endif
  }
  return GetAbsoluteWorkingDirOrCurrentPath(workingDir);
}

std::filesystem::path GetAbsolutePath(const std::filesystem::path& p, std::filesystem::path workingDir)
{
  if (p.is_absolute()) {
    return p;
  }

  return GetAbsoluteWorkingDirOrCurrentPath(workingDir) / p;
}

std::filesystem::path GetOutputBasePath() {
  return std::filesystem::current_path() / GetExecutablePath().filename();
}

}
