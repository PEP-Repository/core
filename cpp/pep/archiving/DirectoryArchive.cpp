#include <pep/archiving/DirectoryArchive.hpp>
#include <cstring>

namespace pep {

DirectoryArchive::DirectoryArchive(const std::filesystem::path& directoryPath) : mDirectoryPath(directoryPath) {
  if (std::filesystem::exists(directoryPath)) {
    throw std::runtime_error("Directory " + directoryPath.string() + " already exists");
  }
  std::filesystem::create_directory(directoryPath);
}

void DirectoryArchive::nextEntry(const std::filesystem::path& path, int64_t size) {
  if (mCurrentFile.is_open()) {
    mCurrentFile.close();
  }
  std::filesystem::path nextEntryPath = mDirectoryPath / path;
  std::filesystem::create_directories(nextEntryPath.parent_path());
  mCurrentFile.open(nextEntryPath.string(), std::ios::binary);
  if (!mCurrentFile) {
    throw std::system_error(errno, std::generic_category(), "Failed to open " + nextEntryPath.string());
  }
}

void DirectoryArchive::writeData(const char* c, const std::streamsize l) {
  mCurrentFile.write(c, l);
  if (!mCurrentFile) {
    throw std::runtime_error("Could not write to file.");
  }
}
void DirectoryArchive::writeData(std::string_view data) {
  writeData(data.data(), static_cast<std::streamsize>(data.length()));
}

void DirectoryArchive::closeEntry() {
  if (mCurrentFile.is_open()) {
    mCurrentFile.close();
  }
}

}
