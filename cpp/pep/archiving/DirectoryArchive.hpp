#pragma once

#include <fstream>

#include <pep/archiving/Archive.hpp>
#include <pep/utils/Shared.hpp>

namespace pep {

class DirectoryArchive : public Archive, public SharedConstructor<DirectoryArchive> {
  friend class SharedConstructor<DirectoryArchive>;
public:
  void nextEntry(const std::filesystem::path& path, int64_t size) override;
  void writeData(std::string_view data) override;
  void writeData(const char* c, const std::streamsize l) override;
  void closeEntry() override;
  bool expectsSizeUpFront() override { return false; }

private:
  DirectoryArchive(const std::filesystem::path& directoryPath);

  std::ofstream mCurrentFile;
  std::filesystem::path mDirectoryPath;
};

}
