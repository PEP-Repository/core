#include <pep/archiving/Archive.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cassert>

#include <pep/utils/File.hpp>

namespace pep {

void WriteToArchive(const std::filesystem::path& baseDir, std::shared_ptr<Archive> archive, std::optional<Pseudonymiser> pseudonymiser) {
  assert(std::filesystem::is_directory(baseDir));
  auto end = std::filesystem::recursive_directory_iterator{};
  for (auto iterator = std::filesystem::recursive_directory_iterator(baseDir, std::filesystem::directory_options::follow_directory_symlink); iterator != end; iterator++) {
    const auto& currentPath = iterator->path();
    if (std::filesystem::is_directory(currentPath)) {
      continue;
    }
    auto in = std::ifstream(currentPath.string(), std::ios::binary);
    std::string processedFilename{};

    auto rawFilename = std::istringstream(currentPath.lexically_relative(baseDir).string());
    if (pseudonymiser.has_value()) {
      auto writeToFilename = [&processedFilename](const char* c, const std::streamsize l) {processedFilename.append(c, static_cast<size_t>(l)); };
      pseudonymiser->pseudonymise(rawFilename, writeToFilename);
    }
    else {
      processedFilename = currentPath.lexically_relative(baseDir).string();
    }
    archive->nextEntry(processedFilename, static_cast<int64_t>(std::filesystem::file_size(currentPath)));
    auto writeToArchive = [&archive](const char* c, const std::streamsize l) { archive->writeData(c, l); };
    try {
      if (pseudonymiser.has_value()) {
        pseudonymiser->pseudonymise(in, writeToArchive);
      }
      else {
        IstreamToDestination(in, writeToArchive);
      }
    }
    catch (std::exception& e) {
        throw std::runtime_error("Encountered error trying to read file: " + currentPath.string() + ". Error: " + std::string(e.what()));
    }
    in.close();
    archive->closeEntry();
  }
}

}
