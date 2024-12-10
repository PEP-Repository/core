#pragma once

#include <filesystem>

namespace pep {

std::filesystem::path GetWorkingDirectory();
void SetWorkingDirectory(const std::filesystem::path& directory);

}
