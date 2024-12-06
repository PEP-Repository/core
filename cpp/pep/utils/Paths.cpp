#include <pep/utils/Paths.hpp>
#include <pep/utils/WorkingDirectory.hpp>
#include <pep/utils/Log.hpp>

#include <boost/dll.hpp>

namespace pep {

std::filesystem::path GetExecutablePath() {
  return boost::dll::program_location();
}

std::filesystem::path GetResourceWorkingDirForOS(const std::filesystem::path& resourcePath) {
  std::filesystem::path workingDir;
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
  return GetAbsolutePath(resourcePath, workingDir);
}

std::filesystem::path GetAbsolutePath(const std::filesystem::path& p, std::filesystem::path workingDir)
{
  if (p.is_absolute()) {
    return p;
  }

  //NOLINTNEXTLINE(concurrency-mt-unsafe) std::getenv is thread safe as long as we do not setenv/unsetenv/putenv
  if (std::getenv("PEP_USE_CURRENT_PATH") == nullptr) {
    if (workingDir.empty()) {
      workingDir = std::filesystem::canonical(GetExecutablePath()).parent_path();
    }
  } else {
    workingDir = std::filesystem::current_path();
  }

  return std::filesystem::absolute(workingDir / p);
}

std::filesystem::path GetOutputBasePath() {
  auto fileName = GetExecutablePath().filename();
  auto directory = GetWorkingDirectory();
  return directory / fileName;
}

}
