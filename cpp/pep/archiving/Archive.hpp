#pragma once

#include <optional>
#include <memory>

#include <filesystem>
#include <pep/archiving/Pseudonymiser.hpp>

namespace pep {

class Archive {
public:
  virtual ~Archive() = default;
  virtual void nextEntry(const std::filesystem::path& path, int64_t size) = 0;
  virtual void writeData(const char* c, const std::streamsize l) = 0;
  virtual void writeData(std::string_view data) = 0;
  virtual void closeEntry() = 0;
  virtual bool expectsSizeUpFront() = 0;
/* \brief Iterates over all files in the given baseDir and its subdirectories and writes it to a archive. An optional pseudonymiser can
* be given ensuring pseudonymisation of filenames and contents.
*/
};

void WriteToArchive(const std::filesystem::path& baseDir, std::shared_ptr<Archive> archive, std::optional<Pseudonymiser> pseudonymiser);
}
