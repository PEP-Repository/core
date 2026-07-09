#include <pep/archiving/DirectoryArchive.hpp>

#include <pep/utils/SafePath.hpp>

namespace pep {

DirectoryArchive::DirectoryArchive(const std::filesystem::path& directoryPath) : directoryPath_(directoryPath) {
  if (std::filesystem::exists(directoryPath)) {
    throw std::runtime_error("Directory " + directoryPath.string() + " already exists");
  }
  std::filesystem::create_directory(directoryPath);
}

void DirectoryArchive::nextEntry(const SafePath& path, int64_t size) {
  if (currentFile_.is_open()) {
    currentFile_.close();
  }
  std::filesystem::path nextEntryPath = directoryPath_ / path;
  std::filesystem::create_directories(nextEntryPath.parent_path());
  currentFile_.open(nextEntryPath.string(), std::ios::binary);
  if (!currentFile_) {
    throw std::system_error(errno, std::generic_category(), "Failed to open " + nextEntryPath.string());
  }
}

void DirectoryArchive::writeData(const char* c, const std::streamsize l) {
  currentFile_.write(c, l);
  if (!currentFile_) {
    throw std::runtime_error("Could not write to file.");
  }
}
void DirectoryArchive::writeData(std::string_view data) {
  writeData(data.data(), static_cast<std::streamsize>(data.length()));
}

void DirectoryArchive::closeEntry() {
  if (currentFile_.is_open()) {
    currentFile_.close();
  }
}

}
