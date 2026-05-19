#include <pep/archiving/Archive.hpp>

#include <fstream>
#include <sstream>
#include <filesystem>
#include <numeric>
#include <ranges>
#include <cassert>

#include <pep/utils/File.hpp>

using namespace std::ranges;

namespace pep {

void WriteToArchive(const std::filesystem::path& baseDir, std::shared_ptr<Archive> archive, std::optional<Pseudonymiser> pseudonymiser) {
  assert(std::filesystem::is_directory(baseDir));
  auto end = std::filesystem::recursive_directory_iterator{};
  for (auto iterator = std::filesystem::recursive_directory_iterator(baseDir, std::filesystem::directory_options::follow_directory_symlink); iterator != end; iterator++) {
    const auto& currentPath = iterator->path();
    if (std::filesystem::is_directory(currentPath)) {
      continue;
    }
    auto in = std::ifstream(currentPath, std::ios::binary);

    SafeRelativeFilePath processedRelativePath;
    auto rawRelativePath = currentPath.lexically_relative(baseDir);
    if (pseudonymiser.has_value()) {
      for (const std::filesystem::path& pathSegment : rawRelativePath) {
        std::istringstream pathSegmentStreamIn(pathSegment.string());
        std::ostringstream pathSegmentStreamOut;
        const auto writeToFilename = [&](const char* c, const std::streamsize l) { pathSegmentStreamOut.write(c, l); };
        pseudonymiser->pseudonymise(pathSegmentStreamIn, writeToFilename);
        processedRelativePath = processedRelativePath / SafeFileName(std::move(pathSegmentStreamOut).str());
      }
    } else {
      processedRelativePath = SafeRelativeFilePath(std::move(rawRelativePath));
    }

    archive->nextEntry(processedRelativePath, static_cast<int64_t>(std::filesystem::file_size(currentPath)));
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
