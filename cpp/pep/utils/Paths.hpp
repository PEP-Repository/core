#pragma once

#include <filesystem>

namespace pep {

std::filesystem::path GetExecutablePath();

// On MacOS, the cli and assessor applications are bundles ith a different 
// resource path than the executable. This function will return the correct path
// of the Resources directory in the bundle and call resolveAbsolutePath().
std::filesystem::path GetResourceWorkingDirForOS();

// Returns an absolute path for p relative to the executable.
// Useful for loading configuration files, and so on.
// If the environment variable PEP_USE_CURRENT_PATH is set, 
// it will compute the absolute path relative to the regular current 
// working directory instead.
std::filesystem::path GetAbsolutePath(
  const std::filesystem::path& p, std::filesystem::path workingDir = "");

// Returns a partial path to be used for application output such as log files.
std::filesystem::path GetOutputBasePath();

}
